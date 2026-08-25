#include "message.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <openssl/rand.h>
#include <vector>

namespace lantalk {

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string unescapeJson(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '\\' && i + 1 < s.length()) {
            ++i;
            if (s[i] == '"') out += '"';
            else if (s[i] == '\\') out += '\\';
            else if (s[i] == 'n') out += '\n';
            else if (s[i] == 'r') out += '\r';
            else if (s[i] == 't') out += '\t';
            else out += s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string ChatMessage::toJson() const {
    return std::string("{") +
        "\"msgId\":\"" + escapeJson(msgId) + "\"," +
        "\"senderId\":\"" + escapeJson(senderId) + "\"," +
        "\"senderName\":\"" + escapeJson(senderName) + "\"," +
        "\"timestampUtc\":" + std::to_string(timestampUtc) + "," +
        "\"text\":\"" + escapeJson(text) + "\"" +
        "}";
}

static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = pos;
    while (end < json.length()) {
        if (json[end] == '"' && (end == 0 || json[end-1] != '\\')) break;
        ++end;
    }
    return unescapeJson(json.substr(pos, end - pos));
}

static int64_t extractJsonInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.length();
    size_t end = pos;
    while (end < json.length() && (std::isdigit(json[end]) || json[end] == '-')) {
        ++end;
    }
    try {
        return std::stoll(json.substr(pos, end - pos));
    } catch (...) {
        return 0;
    }
}

std::optional<ChatMessage> ChatMessage::fromJson(const std::string& json) {
    ChatMessage msg;
    msg.msgId = extractJsonString(json, "msgId");
    msg.senderId = extractJsonString(json, "senderId");
    msg.senderName = extractJsonString(json, "senderName");
    msg.text = extractJsonString(json, "text");
    msg.timestampUtc = extractJsonInt(json, "timestampUtc");
    if (msg.msgId.empty() || msg.senderId.empty()) return std::nullopt;
    return msg;
}

std::string ChatMessage::formatForDisplay() const {
    std::time_t t = static_cast<std::time_t>(timestampUtc);
    std::tm* tm_info = std::localtime(&t);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "[%H:%M:%S]", tm_info);
    return std::string(buffer);
}

std::string ChatMessage::generateId() {
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        // Fallback or error
        return "";
    }
    // UUID v4 format
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    
    char hex[37];
    snprintf(hex, sizeof(hex), 
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(hex);
}

int64_t ChatMessage::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

} // namespace lantalk
