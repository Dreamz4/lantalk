#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <cstdint>

namespace lantalk {

struct ParsedArgs {
    std::string command;                   // e.g. "listen", "connect", "scan"
    std::vector<std::string> positional;   // positional args
    std::map<std::string, std::string> flags; // --port 5050 etc.

    bool hasFlag(const std::string& name) const;
    std::optional<std::string> getFlag(const std::string& name) const;
    std::optional<uint16_t> getFlagAsPort(const std::string& name) const;
    std::optional<int> getFlagAsInt(const std::string& name) const;
};

class CommandParser {
public:
    // Parse argc/argv into ParsedArgs
    static ParsedArgs parse(int argc, const char* const* argv);

    // Print help text
    static void printHelp();
    static void printVersion();
};

} // namespace lantalk
