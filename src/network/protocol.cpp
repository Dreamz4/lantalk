#include "protocol.hpp"
#include "socket_types.hpp"
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace lantalk {

std::string messageTypeName(MessageType type) {
    switch(type) {
        case MessageType::HELLO: return "HELLO";
        case MessageType::AUTH_REQUEST: return "AUTH_REQUEST";
        case MessageType::AUTH_RESPONSE: return "AUTH_RESPONSE";
        case MessageType::AUTH_OK: return "AUTH_OK";
        case MessageType::AUTH_FAIL: return "AUTH_FAIL";
        case MessageType::CHAT_MESSAGE: return "CHAT_MESSAGE";
        case MessageType::CHAT_RECEIPT: return "CHAT_RECEIPT";
        case MessageType::PING: return "PING";
        case MessageType::PONG: return "PONG";
        case MessageType::PEER_LIST: return "PEER_LIST";
        case MessageType::DISCONNECT: return "DISCONNECT";
        case MessageType::ERROR_MSG: return "ERROR_MSG";
        case MessageType::FILE_OFFER: return "FILE_OFFER";
        case MessageType::FILE_ACCEPT: return "FILE_ACCEPT";
        case MessageType::FILE_CHUNK: return "FILE_CHUNK";
        case MessageType::FILE_COMPLETE: return "FILE_COMPLETE";
        default: return "UNKNOWN";
    }
}

MessageType messageTypeFromByte(uint8_t byte) {
    switch(byte) {
        case 0x01: return MessageType::HELLO;
        case 0x02: return MessageType::AUTH_REQUEST;
        case 0x03: return MessageType::AUTH_RESPONSE;
        case 0x04: return MessageType::AUTH_OK;
        case 0x05: return MessageType::AUTH_FAIL;
        case 0x10: return MessageType::CHAT_MESSAGE;
        case 0x11: return MessageType::CHAT_RECEIPT;
        case 0x20: return MessageType::PING;
        case 0x21: return MessageType::PONG;
        case 0x30: return MessageType::PEER_LIST;
        case 0x40: return MessageType::DISCONNECT;
        case 0x50: return MessageType::ERROR_MSG;
        case 0x60: return MessageType::FILE_OFFER;
        case 0x61: return MessageType::FILE_ACCEPT;
        case 0x62: return MessageType::FILE_CHUNK;
        case 0x63: return MessageType::FILE_COMPLETE;
        default: return MessageType::UNKNOWN;
    }
}

std::vector<uint8_t> FrameHeader::serialize() const {
    std::vector<uint8_t> out(kFrameHeaderSize);
    uint32_t netMagic = htonl(magic);
    std::memcpy(out.data(), &netMagic, 4);
    out[4] = version;
    out[5] = static_cast<uint8_t>(type);
    uint32_t netLen = htonl(payloadLength);
    std::memcpy(out.data() + 6, &netLen, 4);
    uint16_t netFlags = htons(flags);
    std::memcpy(out.data() + 10, &netFlags, 2);
    // reserved is not strictly checked or populated
    return out;
}

std::optional<FrameHeader> FrameHeader::deserialize(const uint8_t* data, size_t len) {
    if (len < kFrameHeaderSize) return std::nullopt;
    
    FrameHeader h;
    uint32_t netMagic;
    std::memcpy(&netMagic, data, 4);
    h.magic = ntohl(netMagic);
    
    if (h.magic != kProtocolMagic) return std::nullopt;
    
    h.version = data[4];
    if (h.version != kProtocolVersion) return std::nullopt;
    
    h.type = messageTypeFromByte(data[5]);
    
    uint32_t netLen;
    std::memcpy(&netLen, data + 6, 4);
    h.payloadLength = ntohl(netLen);
    
    uint16_t netFlags;
    std::memcpy(&netFlags, data + 10, 2);
    h.flags = ntohs(netFlags);
    
    return h;
}

Frame Frame::create(MessageType type, std::vector<uint8_t> payload) {
    Frame f;
    f.header.type = type;
    f.header.payloadLength = static_cast<uint32_t>(payload.size());
    f.payload = std::move(payload);
    return f;
}

Frame Frame::create(MessageType type, const std::string& jsonPayload) {
    std::vector<uint8_t> payload(jsonPayload.begin(), jsonPayload.end());
    return create(type, std::move(payload));
}

std::vector<uint8_t> Frame::serialize() const {
    std::vector<uint8_t> data = header.serialize();
    data.insert(data.end(), payload.begin(), payload.end());
    return data;
}

std::string Frame::payloadAsString() const {
    return std::string(payload.begin(), payload.end());
}

FrameReader::FrameReader(size_t maxPayloadSize) 
    : maxPayloadSize_(maxPayloadSize) {}

std::vector<Frame> FrameReader::feed(const uint8_t* data, size_t len) {
    std::vector<Frame> completeFrames;
    buffer_.insert(buffer_.end(), data, data + len);

    while (true) {
        if (state_ == State::READING_HEADER) {
            if (buffer_.size() < kFrameHeaderSize) {
                break;
            }
            auto optHdr = FrameHeader::deserialize(buffer_.data(), kFrameHeaderSize);
            if (!optHdr) {
                throw std::runtime_error("Protocol violation: invalid magic or version");
            }
            pendingHeader_ = *optHdr;
            if (pendingHeader_.payloadLength > maxPayloadSize_) {
                throw std::runtime_error("Protocol violation: payload too large");
            }
            buffer_.erase(buffer_.begin(), buffer_.begin() + kFrameHeaderSize);
            state_ = State::READING_PAYLOAD;
        } else if (state_ == State::READING_PAYLOAD) {
            if (buffer_.size() < pendingHeader_.payloadLength) {
                break;
            }
            Frame f;
            f.header = pendingHeader_;
            if (pendingHeader_.payloadLength > 0) {
                f.payload.assign(buffer_.begin(), buffer_.begin() + pendingHeader_.payloadLength);
                buffer_.erase(buffer_.begin(), buffer_.begin() + pendingHeader_.payloadLength);
            }
            completeFrames.push_back(std::move(f));
            state_ = State::READING_HEADER;
        }
    }
    return completeFrames;
}

void FrameReader::reset() {
    buffer_.clear();
    state_ = State::READING_HEADER;
}

} // namespace lantalk
