#include "server/gui/discovery.hpp"
#include "shared/util/log.hpp"

#include <cstring>
#include <algorithm>
#include <array>

// Link against ws2_32.lib is handled by CMakeLists, but add a pragma just in case
#pragma comment(lib, "ws2_32.lib")

namespace ud {

// ─── mDNS constants ──────────────────────────────────────────────────────────
static constexpr const char* MDNS_MULTICAST_ADDR = "224.0.0.251";
static constexpr uint16_t    MDNS_PORT           = 5353;
static constexpr const char* SERVICE_NAME        = "_ultradisplay._udp.local";
static constexpr auto        STALE_TIMEOUT       = std::chrono::seconds(10);
static constexpr auto        QUERY_INTERVAL      = std::chrono::seconds(2);

// ─── DNS record types ────────────────────────────────────────────────────────
static constexpr uint16_t DNS_TYPE_PTR = 12;
static constexpr uint16_t DNS_TYPE_SRV = 33;
static constexpr uint16_t DNS_TYPE_TXT = 16;
static constexpr uint16_t DNS_TYPE_A   = 1;
static constexpr uint16_t DNS_CLASS_IN = 1;

// ─── Helpers: big-endian read/write ──────────────────────────────────────────
static inline void write_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}

static inline uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static inline uint32_t read_u32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           static_cast<uint32_t>(p[3]);
}

// ─── Constructor / Destructor ────────────────────────────────────────────────

Discovery::Discovery() {
    // Initialize Winsock
    WSADATA wsa_data{};
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        UD_LOG_ERROR("Discovery: WSAStartup failed with error {}", result);
    }
}

Discovery::~Discovery() {
    stop_scan();
    WSACleanup();
}

// ─── Public API ──────────────────────────────────────────────────────────────

void Discovery::start_scan() {
    if (running_.load()) return;

    running_.store(true);
    thread_ = std::thread(&Discovery::scan_thread_func, this);
    UD_LOG_INFO("Discovery: scan started");
}

void Discovery::stop_scan() {
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
    UD_LOG_INFO("Discovery: scan stopped");
}

std::vector<DiscoveredClient> Discovery::get_clients() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_;
}

void Discovery::set_callback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

// ─── DNS name encoding ──────────────────────────────────────────────────────

int Discovery::encode_dns_name(const char* name, uint8_t* buf, int buf_size) {
    int pos = 0;
    const char* p = name;

    while (*p) {
        const char* dot = strchr(p, '.');
        int label_len = dot ? static_cast<int>(dot - p) : static_cast<int>(strlen(p));
        if (pos + 1 + label_len >= buf_size) return -1;

        buf[pos++] = static_cast<uint8_t>(label_len);
        memcpy(buf + pos, p, label_len);
        pos += label_len;

        if (dot) {
            p = dot + 1;
        } else {
            break;
        }
    }

    if (pos >= buf_size) return -1;
    buf[pos++] = 0; // null terminator
    return pos;
}

// ─── DNS name decoding (with pointer compression support) ────────────────────

std::string Discovery::decode_dns_name(const uint8_t* pkt, int pkt_len, int& offset) {
    std::string result;
    bool jumped = false;
    int original_offset = offset;
    int jumps = 0;

    while (offset < pkt_len) {
        uint8_t len = pkt[offset];

        if (len == 0) {
            offset++;
            break;
        }

        // Pointer compression
        if ((len & 0xC0) == 0xC0) {
            if (offset + 1 >= pkt_len) break;
            int ptr = ((len & 0x3F) << 8) | pkt[offset + 1];
            if (!jumped) {
                original_offset = offset + 2;
            }
            offset = ptr;
            jumped = true;
            jumps++;
            if (jumps > 10) break; // guard against infinite loops
            continue;
        }

        offset++;
        if (offset + len > pkt_len) break;

        if (!result.empty()) result += '.';
        result.append(reinterpret_cast<const char*>(pkt + offset), len);
        offset += len;
    }

    if (jumped) {
        offset = original_offset;
    }

    return result;
}

// ─── mDNS query construction ────────────────────────────────────────────────

bool Discovery::send_mdns_query(SOCKET sock, const sockaddr_in& mcast_addr) {
    // Build a standard mDNS PTR query for _ultradisplay._udp.local
    std::array<uint8_t, 512> pkt{};
    int pos = 0;

    // Transaction ID (0 for mDNS)
    write_u16(pkt.data() + pos, 0); pos += 2;
    // Flags: standard query
    write_u16(pkt.data() + pos, 0); pos += 2;
    // Questions: 1
    write_u16(pkt.data() + pos, 1); pos += 2;
    // Answer RRs: 0
    write_u16(pkt.data() + pos, 0); pos += 2;
    // Authority RRs: 0
    write_u16(pkt.data() + pos, 0); pos += 2;
    // Additional RRs: 0
    write_u16(pkt.data() + pos, 0); pos += 2;

    // Query name: _ultradisplay._udp.local
    int name_len = encode_dns_name(SERVICE_NAME, pkt.data() + pos, static_cast<int>(pkt.size()) - pos);
    if (name_len < 0) return false;
    pos += name_len;

    // Type: PTR (12)
    write_u16(pkt.data() + pos, DNS_TYPE_PTR); pos += 2;
    // Class: IN (1) with unicast-response bit cleared
    write_u16(pkt.data() + pos, DNS_CLASS_IN); pos += 2;

    int sent = sendto(sock, reinterpret_cast<const char*>(pkt.data()), pos, 0,
                      reinterpret_cast<const sockaddr*>(&mcast_addr), sizeof(mcast_addr));
    return sent > 0;
}

// ─── mDNS response parsing ──────────────────────────────────────────────────

bool Discovery::parse_mdns_response(const uint8_t* data, int len, DiscoveredClient& out) {
    if (len < 12) return false;

    // uint16_t tx_id = read_u16(data);
    uint16_t flags = read_u16(data + 2);
    // Must be a response
    if (!(flags & 0x8000)) return false;

    uint16_t qd_count = read_u16(data + 4);
    uint16_t an_count = read_u16(data + 6);
    // uint16_t ns_count = read_u16(data + 8);
    uint16_t ar_count = read_u16(data + 10);

    int offset = 12;

    // Skip questions
    for (uint16_t i = 0; i < qd_count && offset < len; i++) {
        decode_dns_name(data, len, offset);
        offset += 4; // type + class
        if (offset > len) return false;
    }

    bool found_service = false;
    std::string srv_target;

    // Parse answer + additional records
    uint16_t total_rr = an_count + ar_count;
    // We also need to skip authority records but let's include them in the parsing count
    uint16_t ns_count = read_u16(data + 8);
    total_rr += ns_count;

    for (uint16_t i = 0; i < total_rr && offset < len; i++) {
        int name_start = offset;
        std::string rr_name = decode_dns_name(data, len, offset);
        if (offset + 10 > len) break;

        uint16_t rr_type  = read_u16(data + offset); offset += 2;
        uint16_t rr_class = read_u16(data + offset); offset += 2;
        (void)rr_class;
        uint32_t rr_ttl   = read_u32(data + offset); offset += 4;
        (void)rr_ttl;
        uint16_t rd_len   = read_u16(data + offset); offset += 2;

        int rd_start = offset;
        if (offset + rd_len > len) break;

        switch (rr_type) {
        case DNS_TYPE_PTR: {
            // PTR record: points to the service instance name
            int ptr_off = offset;
            std::string instance = decode_dns_name(data, len, ptr_off);
            // Extract device name from instance (e.g., "MyDevice._ultradisplay._udp.local")
            auto dot_pos = instance.find('.');
            if (dot_pos != std::string::npos) {
                out.name = instance.substr(0, dot_pos);
            } else {
                out.name = instance;
            }
            found_service = true;
            break;
        }
        case DNS_TYPE_SRV: {
            // SRV record: priority(2), weight(2), port(2), target(name)
            if (rd_len >= 6) {
                // uint16_t priority = read_u16(data + offset);
                // uint16_t weight   = read_u16(data + offset + 2);
                out.port = read_u16(data + offset + 4);
                int target_off = offset + 6;
                srv_target = decode_dns_name(data, len, target_off);
            }
            break;
        }
        case DNS_TYPE_TXT: {
            // TXT record: series of length-prefixed strings
            int txt_off = offset;
            while (txt_off < offset + rd_len) {
                uint8_t txt_len = data[txt_off++];
                if (txt_off + txt_len > offset + rd_len) break;
                std::string txt(reinterpret_cast<const char*>(data + txt_off), txt_len);
                // Look for "fp=<fingerprint>"
                if (txt.rfind("fp=", 0) == 0) {
                    out.fingerprint = txt.substr(3);
                }
                txt_off += txt_len;
            }
            break;
        }
        case DNS_TYPE_A: {
            // A record: 4-byte IPv4 address
            if (rd_len == 4) {
                char ip_buf[INET_ADDRSTRLEN]{};
                snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u",
                         data[offset], data[offset + 1],
                         data[offset + 2], data[offset + 3]);
                out.ip = ip_buf;
            }
            break;
        }
        default:
            break;
        }

        offset = rd_start + rd_len;
    }

    return found_service && !out.ip.empty();
}

// ─── Stale client pruning ────────────────────────────────────────────────────

void Discovery::prune_stale_clients() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove_if(clients_.begin(), clients_.end(),
        [&](const DiscoveredClient& c) {
            return (now - c.last_seen) > STALE_TIMEOUT;
        });
    if (it != clients_.end()) {
        clients_.erase(it, clients_.end());
    }
}

// ─── Scan thread ─────────────────────────────────────────────────────────────

void Discovery::scan_thread_func() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        UD_LOG_ERROR("Discovery: failed to create socket, error {}", WSAGetLastError());
        return;
    }

    // Allow address reuse
    BOOL reuse = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Bind to INADDR_ANY on mDNS port
    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(MDNS_PORT);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        UD_LOG_WARN("Discovery: bind failed (error {}), trying ephemeral port", WSAGetLastError());
        // Fall back to ephemeral port — we can still send queries and receive unicast responses
        bind_addr.sin_port = 0;
        if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
            UD_LOG_ERROR("Discovery: bind to ephemeral port also failed, error {}", WSAGetLastError());
            closesocket(sock);
            return;
        }
    }

    // Join multicast group
    ip_mreq mreq{};
    inet_pton(AF_INET, MDNS_MULTICAST_ADDR, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));

    // Set receive timeout to 500ms so we don't block forever
    DWORD timeout_ms = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    // Multicast destination
    sockaddr_in mcast_addr{};
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_port = htons(MDNS_PORT);
    inet_pton(AF_INET, MDNS_MULTICAST_ADDR, &mcast_addr.sin_addr);

    auto last_query = std::chrono::steady_clock::now() - QUERY_INTERVAL;

    UD_LOG_INFO("Discovery: scan thread running");

    while (running_.load()) {
        auto now = std::chrono::steady_clock::now();

        // Send a query every QUERY_INTERVAL
        if ((now - last_query) >= QUERY_INTERVAL) {
            send_mdns_query(sock, mcast_addr);
            last_query = now;
        }

        // Try to receive a response
        std::array<uint8_t, 4096> recv_buf{};
        sockaddr_in sender_addr{};
        int sender_len = sizeof(sender_addr);

        int received = recvfrom(sock, reinterpret_cast<char*>(recv_buf.data()),
                                static_cast<int>(recv_buf.size()), 0,
                                reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);

        if (received > 0) {
            DiscoveredClient client;
            if (parse_mdns_response(recv_buf.data(), received, client)) {
                client.last_seen = std::chrono::steady_clock::now();

                // If the parsed response didn't have an IP in an A record,
                // fall back to the sender address
                if (client.ip.empty()) {
                    char addr_buf[INET_ADDRSTRLEN]{};
                    inet_ntop(AF_INET, &sender_addr.sin_addr, addr_buf, sizeof(addr_buf));
                    client.ip = addr_buf;
                }

                bool changed = false;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = std::find_if(clients_.begin(), clients_.end(),
                        [&](const DiscoveredClient& c) { return c.ip == client.ip; });
                    if (it != clients_.end()) {
                        it->last_seen = client.last_seen;
                        if (it->name != client.name || it->port != client.port) {
                            it->name = client.name;
                            it->port = client.port;
                            it->fingerprint = client.fingerprint;
                            changed = true;
                        }
                    } else {
                        clients_.push_back(std::move(client));
                        changed = true;
                    }
                }

                if (changed) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (callback_) callback_();
                }
            }
        }

        // Prune stale clients
        size_t old_size;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            old_size = clients_.size();
        }
        prune_stale_clients();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (clients_.size() != old_size && callback_) {
                callback_();
            }
        }
    }

    // Leave multicast group
    setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
    closesocket(sock);

    UD_LOG_INFO("Discovery: scan thread exiting");
}

} // namespace ud
