#pragma once

#include <cstdint>
#include <span>

namespace ud {

// Zero-copy message structures
#pragma pack(push, 1)

constexpr uint16_t PROTOCOL_MAGIC = 0x5544; // "UD"

enum class PacketType : uint8_t {
    Video = 0,
    Audio,
    FEC,
    Control,
    ACK,
    NACK,
    Cursor
};

struct PacketHeader {
    uint16_t magic;
    PacketType type;
    uint8_t flags; // Bit 0: keyframe, Bit 1: has_roi, Bit 2: hdr, Bit 3: encrypted
    uint32_t sequence_number;
    uint32_t timestamp_us;
    uint16_t frame_number;
    uint8_t slice_index_and_total; // top 4 bits: index, bottom 4 bits: total
    uint8_t fec_group_id;
    uint16_t payload_length;
    uint8_t stream_id; // 0=video, 1=audio, 2=control
    uint8_t reserved_ecn;
};

enum class ControlMessageType : uint8_t {
    Handshake = 0,
    CodecNegotiation,
    SessionStart,
    SessionStop,
    BitrateUpdate,
    DisplayInfo,
    InputEvent,
    CursorUpdate,
    AudioConfig,
    PairingRequest,
    PairingResponse,
    Ping,
    Pong,
    ReceiverReport
};

struct ControlHeader {
    ControlMessageType type;
    uint16_t length;
};

// ... Specific control payloads will be cast from the data following ControlHeader

#pragma pack(pop)

} // namespace ud
