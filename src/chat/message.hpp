#pragma once
#include <string>
#include <cstdint>
#include <chrono>
#include <optional>

namespace lantalk {

struct ChatMessage {
    std::string msgId;          // UUID v4
    std::string senderId;       // device UUID
    std::string senderName;     // display name
    int64_t     timestampUtc;   // Unix timestamp seconds UTC
    std::string text;           // UTF-8 message content

    // Serialize to JSON string for wire protocol
    std::string toJson() const;

    // Deserialize from JSON string
    static std::optional<ChatMessage> fromJson(const std::string& json);

    // Format for display with local time
    std::string formatForDisplay() const;

    // Generate a new message ID
    static std::string generateId();

    // Current UTC timestamp
    static int64_t currentTimestamp();
};

} // namespace lantalk
