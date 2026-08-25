#include "config.hpp"
#include "logging/logger.hpp"
#include <openssl/rand.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace lantalk {

Config::Config() {
    config_.platform = platformName();
    ensureDeviceId();
    ensureDisplayName();
}

std::filesystem::path Config::configDirectory() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::filesystem::path(appdata) / "LanTalk";
    }
    return std::filesystem::current_path() / "config";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / "Library/Application Support/LanTalk";
    }
    return std::filesystem::current_path() / "config";
#else
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        return std::filesystem::path(xdg_config) / "lantalk";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config" / "lantalk";
    }
    return std::filesystem::current_path() / "config";
#endif
}

std::filesystem::path Config::configFilePath() {
    return configDirectory() / "config.ini";
}

void Config::ensureDeviceId() {
    if (config_.deviceId.empty()) {
        config_.deviceId = generateUUID();
    }
}

void Config::ensureDisplayName() {
    if (config_.displayName.empty()) {
        config_.displayName = readHostname();
        if (config_.displayName.empty()) {
            config_.displayName = "UnknownUser";
        }
    }
}

std::string Config::readHostname() {
    char hostname[256];
#ifdef _WIN32
    DWORD size = sizeof(hostname);
    if (GetComputerNameA(hostname, &size)) {
        return std::string(hostname);
    }
#else
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
#endif
    return "";
}

std::string Config::generateUUID() {
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        LOG_ERROR("Failed to generate random bytes for UUID");
        return "00000000-0000-0000-0000-000000000000";
    }

    // UUID v4 format adjustments
    bytes[6] = (bytes[6] & 0x0f) | 0x40; // Version 4
    bytes[8] = (bytes[8] & 0x3f) | 0x80; // Variant 1

    std::ostringstream uuid;
    uuid << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        uuid << std::setw(2) << static_cast<int>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            uuid << "-";
        }
    }
    return uuid.str();
}

std::string Config::platformName() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

// Minimal INI parsing/saving
bool Config::load() {
    auto path = configFilePath();
    if (!std::filesystem::exists(path)) {
        LOG_INFO("Config file not found, creating default.");
        return save();
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Could not open config file for reading");
        return false;
    }

    std::string line;
    std::string currentSection;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim
            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t"));
                s.erase(s.find_last_not_of(" \t") + 1);
            };
            trim(key);
            trim(value);
            
            set(key, value); // naive set, assumes keys are unique globally for now
        }
    }
    
    // Ensure critical fields are set even if missing from config
    ensureDeviceId();
    ensureDisplayName();

    return true;
}

bool Config::save() const {
    auto dir = configDirectory();
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(dir, ec)) {
            LOG_ERROR("Could not create config directory: " + ec.message());
            return false;
        }
    }

    std::ofstream file(configFilePath());
    if (!file.is_open()) {
        LOG_ERROR("Could not open config file for writing");
        return false;
    }

    file << "[identity]\n";
    file << "device_id = " << config_.deviceId << "\n";
    file << "display_name = " << config_.displayName << "\n";
    file << "platform = " << config_.platform << "\n\n";

    file << "[network]\n";
    file << "listen_port = " << config_.listenPort << "\n";
    file << "discovery_port = " << config_.discoveryPort << "\n";
    file << "enable_ipv6 = " << (config_.enableIPv6 ? "true" : "false") << "\n";
    file << "max_message_size = " << config_.maxMessageSize << "\n";
    file << "connect_timeout_secs = " << config_.connectTimeoutSecs << "\n";
    file << "read_timeout_secs = " << config_.readTimeoutSecs << "\n";
    file << "keepalive_interval_secs = " << config_.keepaliveIntervalSecs << "\n\n";

    file << "[discovery]\n";
    file << "discovery_enabled = " << (config_.discoveryEnabled ? "true" : "false") << "\n\n";

    file << "[security]\n";
    file << "psk_hash = " << config_.pskHash << "\n";
    file << "require_auth = " << (config_.requireAuth ? "true" : "false") << "\n\n";

    file << "[logging]\n";
    file << "log_level = " << config_.logLevel << "\n";
    file << "log_file = " << config_.logFile << "\n";

    return true;
}

bool Config::set(const std::string& key, const std::string& value) {
    if (key == "device_id") config_.deviceId = value;
    else if (key == "display_name") config_.displayName = value;
    else if (key == "platform") config_.platform = value;
    else if (key == "listen_port") config_.listenPort = std::stoi(value);
    else if (key == "discovery_port") config_.discoveryPort = std::stoi(value);
    else if (key == "enable_ipv6") config_.enableIPv6 = (value == "true");
    else if (key == "max_message_size") config_.maxMessageSize = std::stoul(value);
    else if (key == "connect_timeout_secs") config_.connectTimeoutSecs = std::stoi(value);
    else if (key == "read_timeout_secs") config_.readTimeoutSecs = std::stoi(value);
    else if (key == "keepalive_interval_secs") config_.keepaliveIntervalSecs = std::stoi(value);
    else if (key == "discovery_enabled") config_.discoveryEnabled = (value == "true");
    else if (key == "psk_hash") config_.pskHash = value;
    else if (key == "require_auth") config_.requireAuth = (value == "true");
    else if (key == "log_level") config_.logLevel = value;
    else if (key == "log_file") config_.logFile = value;
    else return false;
    return true;
}

std::optional<std::string> Config::getAsString(const std::string& key) const {
    if (key == "device_id") return config_.deviceId;
    if (key == "display_name") return config_.displayName;
    if (key == "platform") return config_.platform;
    if (key == "log_level") return config_.logLevel;
    if (key == "log_file") return config_.logFile;
    return std::nullopt;
}

void Config::print() const {
    LOG_INFO("Config: display_name=" + config_.displayName + ", listen_port=" + std::to_string(config_.listenPort));
}

} // namespace lantalk
