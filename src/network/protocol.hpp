#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <optional>
#include <stdexcept>

namespace lantalk {

// Wire protocol constants
inline constexpr uint32_t kProtocolMagic   = 0x4C544C41; // 'LTLA'
inline constexpr uint8_t  kProtocolVersion = 1;
inline constexpr size_t   kFrameHeaderSize = 12;
inline constexpr size_t   kMaxPayloadSize  = 65536;

enum class MessageType : uint8_t {
    HELLO        = 0x01,
    AUTH_REQUEST  = 0x02,
    AUTH_RESPONSE = 0x03,
    AUTH_OK      = 0x04,
    AUTH_FAIL    = 0x05,
    CHAT_MESSAGE = 0x10,
    CHAT_RECEIPT = 0x11,
    PING         = 0x20,
    PONG         = 0x21,
    PEER_LIST    = 0x30,
    DISCONNECT   = 0x40,
    ERROR_MSG    = 0x50,
    // Reserved for future file transfer
    FILE_OFFER   = 0x60,
    FILE_ACCEPT  = 0x61,
    FILE_CHUNK   = 0x62,
    FILE_COMPLETE = 0x63,
    UNKNOWN      = 0xFF
};

std::string messageTypeName(MessageType type);
MessageType messageTypeFromByte(uint8_t byte);

// Frame header (12 bytes)
struct FrameHeader {
    uint32_t magic{kProtocolMagic};
    uint8_t  version{kProtocolVersion};
    MessageType type{MessageType::UNKNOWN};
    uint32_t payloadLength{0};
    uint16_t flags{0};
    uint16_t reserved{0};

    // Serialize to network byte order
    std::vector<uint8_t> serialize() const;

    // Deserialize from 12 bytes of raw data
    // Returns nullopt if magic/version invalid
    static std::optional<FrameHeader> deserialize(const uint8_t* data, size_t len);
};

// Complete frame: header + payload
struct Frame {
    FrameHeader header;
    std::vector<uint8_t> payload;

    // Build a frame ready to send
    static Frame create(MessageType type, std::vector<uint8_t> payload);
    static Frame create(MessageType type, const std::string& jsonPayload);

    // Serialize entire frame for sending
    std::vector<uint8_t> serialize() const;

    // Get payload as string
    std::string payloadAsString() const;
};

// Incremental frame reader - handles partial TCP reads
class FrameReader {
public:
    explicit FrameReader(size_t maxPayloadSize = kMaxPayloadSize);

    // Feed raw bytes. Returns complete frames.
    // Throws std::runtime_error on protocol violations.
    std::vector<Frame> feed(const uint8_t* data, size_t len);

    void reset();

private:
    enum class State { READING_HEADER, READING_PAYLOAD };

    State state_{State::READING_HEADER};
    std::vector<uint8_t> buffer_;
    FrameHeader pendingHeader_;
    size_t maxPayloadSize_;
};

} // namespace lantalk
