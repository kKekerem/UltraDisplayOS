#pragma once

#include "shared/protocol/messages.hpp"
#include <vector>
#include <span>

namespace ud {

enum class FecLevel {
    None,       // < 0.5% loss
    Low,        // 0.5 - 2% loss (XOR, 10% overhead)
    Medium,     // 2 - 5% loss (Reed-Solomon 20,16)
    High        // > 5% loss (Reed-Solomon 10,6)
};

class FecEncoder {
public:
    explicit FecEncoder();
    ~FecEncoder();

    void set_level(FecLevel level);
    
    // Feed data packets, returns FEC packets if a group completes
    std::vector<std::vector<uint8_t>> encode(const PacketHeader& header, std::span<const uint8_t> payload);

private:
    FecLevel current_level_;
    uint8_t current_group_id_;
    std::vector<std::vector<uint8_t>> data_block_;
    
    void encode_xor(std::vector<std::vector<uint8_t>>& output);
    void encode_rs(size_t k, size_t n, std::vector<std::vector<uint8_t>>& output);
};

class FecDecoder {
public:
    explicit FecDecoder();
    ~FecDecoder();

    // Feed received packets (data or parity). Returns recovered data packets if any.
    std::vector<std::vector<uint8_t>> decode(const PacketHeader& header, std::span<const uint8_t> payload);

private:
    // Decodes XOR or RS based on group metadata
};

} // namespace ud
