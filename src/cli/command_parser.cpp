#include "command_parser.hpp"
#include <iostream>
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace lantalk {

bool ParsedArgs::hasFlag(const std::string& name) const {
    return flags.find(name) != flags.end();
}

std::optional<std::string> ParsedArgs::getFlag(const std::string& name) const {
    auto it = flags.find(name);
    if (it != flags.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<uint16_t> ParsedArgs::getFlagAsPort(const std::string& name) const {
    auto val = getFlag(name);
    if (!val) return std::nullopt;
    
    try {
        int port = std::stoi(*val);
        if (port > 0 && port <= 65535) {
            return static_cast<uint16_t>(port);
        }
    } catch (...) {
        // Fallthrough to return nullopt
    }
    return std::nullopt;
}

std::optional<int> ParsedArgs::getFlagAsInt(const std::string& name) const {
    auto val = getFlag(name);
    if (!val) return std::nullopt;
    
    try {
        return std::stoi(*val);
    } catch (...) {
        return std::nullopt;
    }
}

ParsedArgs CommandParser::parse(int argc, const char* const* argv) {
    ParsedArgs args;
    if (argc < 2) {
        return args;
    }

    std::string firstArg = argv[1];
    if (firstArg == "--help" || firstArg == "-h") {
        args.command = "help";
        return args;
    }
    if (firstArg == "--version" || firstArg == "-v") {
        args.command = "version";
        return args;
    }

    args.command = firstArg;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "--") {
            std::string flagName = arg.substr(2);
            if (i + 1 < argc && std::string(argv[i+1]).substr(0, 2) != "--") {
                args.flags[flagName] = argv[i+1];
                ++i; // skip value
            } else {
                args.flags[flagName] = "true";
            }
        } else {
            args.positional.push_back(arg);
        }
    }

    return args;
}

void CommandParser::printHelp() {
    std::cout << "LanTalk - Cross-platform LAN messaging application\n\n"
              << "Usage: lantalk <command> [options]\n\n"
              << "Commands:\n"
              << "  listen          Start in listening mode waiting for peers\n"
              << "    --port N      Set listening port (default: 5050)\n"
              << "    --verbose     Enable verbose logging\n"
              << "    --quiet       Disable all non-error logging\n"
              << "    --log-level L Set log level (DEBUG, INFO, WARN, ERROR)\n\n"
              << "  connect HOST    Connect to a specific host\n"
              << "    --port N      Set port to connect to (default: 5050)\n"
              << "    --verbose     Enable verbose logging\n"
              << "    --quiet       Disable all non-error logging\n\n"
              << "  scan            Scan local network for peers\n"
              << "    --timeout N   Scan timeout in seconds (default: 5)\n\n"
              << "  config          Manage application configuration\n"
              << "    set KEY VAL   Set configuration key to value\n"
              << "    get KEY       Get configuration key value\n"
              << "    show          Show all configuration values\n\n"
              << "  version         Show application version\n"
              << "  status          Show application status\n"
              << "  peers           List connected peers\n\n"
              << "  --help          Show this help message\n"
              << "  --version       Show application version\n";
}

void CommandParser::printVersion() {
    std::cout << "LanTalk v1.0.0\n";
}

} // namespace lantalk
