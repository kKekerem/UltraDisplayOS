#include "server_transport.hpp"
#include <iostream>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace ud {

struct ServerState {
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::vector<uint8_t>> queue;
    std::vector<uint8_t> current_span_buffer;
};

static std::mutex g_server_state_mtx;
static std::unordered_map<const ServerTransport*, std::unique_ptr<ServerState>> g_server_states;

static ServerState& get_state(const ServerTransport* transport) {
    std::lock_guard<std::mutex> lock(g_server_state_mtx);
    auto it = g_server_states.find(transport);
    if (it == g_server_states.end()) {
        it = g_server_states.emplace(transport, std::make_unique<ServerState>()).first;
    }
    return *it->second;
}

static void remove_state(const ServerTransport* transport) {
    std::lock_guard<std::mutex> lock(g_server_state_mtx);
    g_server_states.erase(transport);
}

ServerTransport::ServerTransport() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    get_state(this);
}

ServerTransport::~ServerTransport() {
    running_ = false;
    if (iocp_) {
        PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (iocp_) {
        CloseHandle(iocp_);
        iocp_ = nullptr;
    }
    remove_state(this);
    WSACleanup();
}

Result<void> ServerTransport::init(uint16_t bind_port) {
    socket_ = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket_ == INVALID_SOCKET) {
        return Error(ErrorCode::NetworkError, "Failed to create socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        return Error(ErrorCode::NetworkError, "Failed to bind socket");
    }

    iocp_ = CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket_), nullptr, 0, 1);
    if (!iocp_) {
        return Error(ErrorCode::SystemError, "Failed to create IOCP");
    }

    running_ = true;
    worker_thread_ = std::thread(&ServerTransport::worker_loop, this);

    post_receive();

    return Result<void>();
}

void ServerTransport::post_receive() {
    auto* ctx = new IoContext{};
    ZeroMemory(&ctx->overlapped, sizeof(WSAOVERLAPPED));
    ctx->wsa_buf.buf = reinterpret_cast<char*>(ctx->buffer);
    ctx->wsa_buf.len = sizeof(ctx->buffer);
    ctx->is_read = true;

    DWORD bytes = 0;
    DWORD flags = 0;
    if (WSARecv(socket_, &ctx->wsa_buf, 1, &bytes, &flags, &ctx->overlapped, nullptr) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            delete ctx;
        }
    }
}

void ServerTransport::worker_loop() {
    while (running_) {
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL res = GetQueuedCompletionStatus(iocp_, &bytes_transferred, &completion_key, &overlapped, INFINITE);

        if (!running_) break;

        if (!res) {
            if (overlapped) {
                delete reinterpret_cast<IoContext*>(overlapped);
            }
            continue;
        }

        if (overlapped) {
            auto* ctx = reinterpret_cast<IoContext*>(overlapped);
            
            if (ctx->is_read && bytes_transferred > 0) {
                current_stats_.bytes_received += bytes_transferred;

                std::vector<uint8_t> data(ctx->buffer, ctx->buffer + bytes_transferred);
                
                auto& state = get_state(this);
                {
                    std::lock_guard<std::mutex> lock(state.mtx);
                    state.queue.push(std::move(data));
                }
                state.cv.notify_one();

                post_receive();
            }

            delete ctx;
        }
    }
}

Result<void> ServerTransport::send(std::span<const uint8_t> payload, uint8_t stream_id) {
    (void)stream_id;
    auto* ctx = new IoContext{};
    ZeroMemory(&ctx->overlapped, sizeof(WSAOVERLAPPED));
    
    size_t copy_size = std::min(payload.size(), sizeof(ctx->buffer));
    std::memcpy(ctx->buffer, payload.data(), copy_size);
    
    ctx->wsa_buf.buf = reinterpret_cast<char*>(ctx->buffer);
    ctx->wsa_buf.len = static_cast<ULONG>(copy_size);
    ctx->is_read = false;

    DWORD bytes_sent = 0;
    if (WSASend(socket_, &ctx->wsa_buf, 1, &bytes_sent, 0, &ctx->overlapped, nullptr) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            delete ctx;
            return Error(ErrorCode::SystemError, "WSASend failed");
        }
    }

    current_stats_.bytes_sent += copy_size;
    return Result<void>();
}

Result<std::span<const uint8_t>> ServerTransport::receive(uint32_t timeout_us) {
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

void ServerTransport::set_bandwidth_limit(uint64_t bps) {
    (void)bps;
}

void ServerTransport::enable_multipath(bool enable) {
    (void)enable;
}

void ServerTransport::set_dscp(uint8_t dscp_value) {
    int tos = dscp_value << 2;
    setsockopt(socket_, IPPROTO_IP, IP_TOS, reinterpret_cast<const char*>(&tos), sizeof(tos));
}

TransportStats ServerTransport::stats() const {
    return current_stats_;
}

} // namespace ud
