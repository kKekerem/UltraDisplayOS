#pragma once

// winsock2.h MUST come before windows.h to avoid redefinition errors
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ud {

struct DiscoveredClient {
    std::string name;
    std::string ip;
    uint16_t    port{0};
    std::string fingerprint;
    std::chrono::steady_clock::time_point last_seen;
};

class Discovery {
public:
    Discovery();
    ~Discovery();

    Discovery(const Discovery&) = delete;
    Discovery& operator=(const Discovery&) = delete;

    /// Start scanning for clients on the network (spawns a background thread).
    void start_scan();

    /// Stop the background scan thread.
    void stop_scan();

    /// Thread-safe: returns a snapshot of currently discovered clients.
    std::vector<DiscoveredClient> get_clients() const;

    /// Set a callback invoked (from the scan thread) whenever the client list changes.
    void set_callback(std::function<void()> cb);

private:
    void scan_thread_func();
    bool send_mdns_query(SOCKET sock, const sockaddr_in& mcast_addr);
    bool parse_mdns_response(const uint8_t* data, int len, DiscoveredClient& out);
    void prune_stale_clients();

    // Helpers for mDNS name encoding / decoding
    static int encode_dns_name(const char* name, uint8_t* buf, int buf_size);
    static std::string decode_dns_name(const uint8_t* pkt, int pkt_len, int& offset);

    mutable std::mutex mutex_;
    std::vector<DiscoveredClient> clients_;
    std::function<void()> callback_;

    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace ud
