#include "terminal_ui.hpp"
#include <iostream>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace lantalk {

TerminalUI::TerminalUI(const AppConfig& config, ChatSession& session)
    : config_(config), session_(session) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

TerminalUI::~TerminalUI() {
    stop();
}

void TerminalUI::run() {
    running_ = true;
    std::string line;
    
    // Initial prompt display
    {
        std::lock_guard<std::mutex> lock(printMutex_);
        std::cout << inputPrompt_ << std::flush;
    }

    while (running_) {
        // Read input char by char to allow intercepting for redraws is complex
        // We use std::getline which may slightly glitch if typing concurrently
        // but it's basic enough for this setup.
        if (!std::getline(std::cin, line)) {
            break; // EOF
        }

        if (line.empty()) continue;

        if (line[0] == '/') {
            if (!handleCommand(line)) {
                break;
            }
        } else {
            // Broadcast message to all connected peers
            session_.broadcast(line);
        }

        {
            std::lock_guard<std::mutex> lock(printMutex_);
            std::cout << inputPrompt_ << std::flush;
        }
    }
    running_ = false;
}

void TerminalUI::printMessage(const std::string& line) {
    std::lock_guard<std::mutex> lock(printMutex_);
    clearLine();
    std::cout << line << "\n";
    redisplayInputLine(""); // Simplified, since we don't have unbuffered input hook
}

void TerminalUI::printInfo(const std::string& line) {
    std::lock_guard<std::mutex> lock(printMutex_);
    clearLine();
    std::cout << "[INFO] " << line << "\n";
    redisplayInputLine("");
}

void TerminalUI::printError(const std::string& line) {
    std::lock_guard<std::mutex> lock(printMutex_);
    clearLine();
    std::cout << "\033[31m[ERROR] " << line << "\033[0m\n";
    redisplayInputLine("");
}

void TerminalUI::printBanner(const std::string& peerName, const std::string& address) {
    std::lock_guard<std::mutex> lock(printMutex_);
    clearLine();
    std::cout << "========================================\n"
              << " LanTalk v1.0.0\n"
              << " Connected to: " << peerName << "\n"
              << " Address: " << address << "\n"
              << "========================================\n";
    redisplayInputLine("");
}

void TerminalUI::printListenBanner(const std::string& address) {
    std::lock_guard<std::mutex> lock(printMutex_);
    clearLine();
    std::cout << "========================================\n"
              << " LanTalk v1.0.0\n"
              << " Listening on: " << address << "\n"
              << " Waiting for peers...\n"
              << "========================================\n";
    redisplayInputLine("");
}

void TerminalUI::stop() {
    running_ = false;
}

bool TerminalUI::handleCommand(const std::string& cmd) {
    if (cmd == "/quit" || cmd == "/exit") {
        printInfo("Exiting...");
        return false;
    } else if (cmd == "/help") {
        printInfo("Available commands:");
        printInfo("  /help      - show available commands");
        printInfo("  /peers     - list connected peers with addresses");
        printInfo("  /info      - show local device info");
        printInfo("  /clear     - clear terminal screen");
        printInfo("  /reconnect - attempt reconnect");
        printInfo("  /quit, /exit - graceful disconnect and exit");
    } else if (cmd == "/peers") {
        printInfo("Connected peers (TODO: Implement listing from session)");
    } else if (cmd == "/info") {
        printInfo("Device ID: " + config_.deviceId);
        printInfo("Display Name: " + config_.displayName);
    } else if (cmd == "/clear") {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    } else if (cmd == "/reconnect") {
        printInfo("Attempting reconnect (TODO: Implement)");
    } else {
        printError("Unknown command: " + cmd + ". Type /help for list of commands.");
    }
    return true;
}

void TerminalUI::clearInputLine() {
    clearLine();
}

void TerminalUI::redisplayInputLine(const std::string& currentInput) {
    std::cout << inputPrompt_ << currentInput << std::flush;
}

bool TerminalUI::supportsAnsi() const {
#ifdef _WIN32
    return true; // Enabled in constructor
#else
    return isatty(fileno(stdout));
#endif
}

void TerminalUI::clearLine() {
    if (supportsAnsi()) {
        std::cout << "\r\033[2K"; // Carriage return + clear line
    } else {
        std::cout << "\r                                                                                \r";
    }
}

void TerminalUI::moveCursorUp(int lines) {
    if (supportsAnsi()) {
        std::cout << "\033[" << lines << "A";
    }
}

} // namespace lantalk
