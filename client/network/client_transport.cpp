#include "client_transport.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <unordered_map>

namespace ud {

struct ClientState {
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::vector<uint8_t>> queue;
    std::vector<uint8_t> current_span_buffer;
};

static std::mutex g_state_mtx;
static std::unordered_map<const ClientTransport*, std::unique_ptr<ClientState>> g_states;

static ClientState& get_state(const ClientTransport* transport) {
    std::lock_guard<std::mutex> lock(g_state_mtx);
    auto it = g_states.find(transport);
    if (it == g_states.end()) {
        it = g_states.emplace(transport, std::make_unique<ClientState>()).first;
    }
    return *it->second;
}

static void remove_state(const ClientTransport* transport) {
    std::lock_guard<std::mutex> lock(g_state_mtx);
    g_states.erase(transport);
}

ClientTransport::ClientTransport() {
    get_state(this);
}

ClientTransport::~ClientTransport() {
    running_ = false;
    if (socket_fd_ != -1) {
        shutdown(socket_fd_, SHUT_RDWR);
    }
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    if (socket_fd_ != -1) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    io_uring_queue_exit(&ring_);
    if (iovecs_) {
        delete[] iovecs_;
        iovecs_ = nullptr;
    }
    remove_state(this);
}

Result<void> ClientTransport::init(uint16_t bind_port) {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return Error(ErrorCode::NetworkError, "Failed to create socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return Error(ErrorCode::NetworkError, "Failed to bind socket");
    }

    try {
        setup_uring();
    } catch (const std::exception& e) {
        return Error(ErrorCode::SystemError, e.what());
    }

    running_ = true;
    poll_thread_ = std::thread(&ClientTransport::poll_loop, this);

    return Result<void>();
}

void ClientTransport::setup_uring() {
    struct io_uring_params params{};
    params.flags |= IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2000;

    if (io_uring_queue_init_params(256, &ring_, &params) < 0) {
        throw std::runtime_error("io_uring_queue_init_params failed");
    }

    recv_buffers_.resize(64, std::vector<uint8_t>(2048));
    iovecs_ = new iovec[64];
    for (size_t i = 0; i < 64; ++i) {
        iovecs_[i].iov_base = recv_buffers_[i].data();
        iovecs_[i].iov_len = recv_buffers_[i].size();
    }

    if (io_uring_register_buffers(&ring_, iovecs_, 64) < 0) {
        throw std::runtime_error("io_uring_register_buffers failed");
    }

    submit_recv_multishot();
}

void ClientTransport::submit_recv_multishot() {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) return;

    io_uring_prep_readv(sqe, socket_fd_, iovecs_, 64, 0);
    io_uring_submit(&ring_);
}

void ClientTransport::poll_loop() {
    while (running_) {
        struct io_uring_cqe* cqe;
        struct __kernel_timespec ts{};
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000; // 100ms
        
        if (io_uring_wait_cqe_timeout(&ring_, &cqe, &ts) < 0) {
            continue;
        }

        if (cqe->res > 0) {
            current_stats_.bytes_received += cqe->res;
            
            std::vector<uint8_t> data(static_cast<const uint8_t*>(iovecs_[0].iov_base), 
                                      static_cast<const uint8_t*>(iovecs_[0].iov_base) + cqe->res);
            
            auto& state = get_state(this);
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.queue.push(std::move(data));
            }
            state.cv.notify_one();
        }

        io_uring_cqe_seen(&ring_, cqe);
        submit_recv_multishot();
    }
}

Result<void> ClientTransport::send(std::span<const uint8_t> payload, uint8_t stream_id) {
    (void)stream_id;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        return Error(ErrorCode::SystemError, "SQE queue full");
    }

    io_uring_prep_send(sqe, socket_fd_, payload.data(), payload.size(), 0);
    io_uring_submit(&ring_);

    current_stats_.bytes_sent += payload.size();
    return Result<void>();
}

Result<std::span<const uint8_t>> ClientTransport::receive(uint32_t timeout_us) {
    auto& state = get_state(this);
    std::unique_lock<std::mutex> lock(state.mtx);
    
    if (state.queue.empty()) {
        if (timeout_us == 0) {
            return Error(ErrorCode::Timeout, "No data");
        }
        if (!state.cv.wait_for(lock, std::chrono::microseconds(timeout_us), [&state]() { return !state.queue.empty(); })) {
            return Error(ErrorCode::Timeout, "Timeout waiting for data");
        }
    }
    
    state.current_span_buffer = std::move(state.queue.front());
    state.queue.pop();
    
    return std::span<const uint8_t>(state.current_span_buffer.data(), state.current_span_buffer.size());
}

void ClientTransport::set_bandwidth_limit(uint64_t bps) {
    (void)bps;
}

void ClientTransport::enable_multipath(bool enable) {
    (void)enable;
}

void ClientTransport::set_dscp(uint8_t dscp_value) {
    int tos = dscp_value << 2;
    setsockopt(socket_fd_, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
}

TransportStats ClientTransport::stats() const {
    return current_stats_;
}

} // namespace ud
