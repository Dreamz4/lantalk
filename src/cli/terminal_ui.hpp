#pragma once
#include "../chat/chat_session.hpp"
#include "../network/tcp_server.hpp"
#include "../network/tcp_client.hpp"
#include "../discovery/lan_discovery.hpp"
#include "../config/config.hpp"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace lantalk {

class TerminalUI {
public:
    explicit TerminalUI(const AppConfig& config, ChatSession& session);
    ~TerminalUI();

    // Run the interactive chat input loop (blocks until /quit)
    void run();

    // Print a message to terminal without corrupting input line
    void printMessage(const std::string& line);

    // Print a system notification
    void printInfo(const std::string& line);
    void printError(const std::string& line);

    // Print connection banner
    void printBanner(const std::string& peerName, const std::string& address);
    void printListenBanner(const std::string& address);

    // Stop the UI
    void stop();

private:
    const AppConfig& config_;
    ChatSession& session_;
    std::atomic<bool> running_{false};
    mutable std::mutex printMutex_;
    std::string inputPrompt_{"You: "};
    std::string currentInput_;

    // Process a slash command entered by user
    // Returns false to exit
    bool handleCommand(const std::string& cmd);

    // Get current input line and clear it
    void clearInputLine();
    void redisplayInputLine(const std::string& currentInput);

    // ANSI escape support
    bool supportsAnsi() const;
    void clearLine();
    void moveCursorUp(int lines);
};

} // namespace lantalk
