// LanTalk - LAN Messaging Application
// Entry point: argument dispatch and lifecycle management

#include "logging/logger.hpp"
#include "config/config.hpp"
#include "network/socket_types.hpp"
#include "network/tcp_server.hpp"
#include "network/tcp_client.hpp"
#include "network/connection.hpp"
#include "chat/chat_session.hpp"
#include "discovery/lan_discovery.hpp"
#include "cli/command_parser.hpp"
#include "cli/terminal_ui.hpp"

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <memory>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

namespace {

std::atomic<bool> g_shutdown{false};
lantalk::TerminalUI* g_ui_ptr = nullptr;

void signalHandler(int /*sig*/) {
    g_shutdown = true;
    if (g_ui_ptr) {
        g_ui_ptr->stop();
    }
}

void setupSignalHandlers() {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifndef _WIN32
    std::signal(SIGHUP,  signalHandler);
    std::signal(SIGPIPE, SIG_IGN); // Ignore broken pipe
#endif
}

// ---- Command implementations -----------------------------------------------

void runListen(lantalk::Config& cfg, const lantalk::ParsedArgs& args) {
    using namespace lantalk;
    AppConfig& config = cfg.get();

    // Override port if supplied
    if (auto port = args.getFlagAsPort("port")) {
        config.listenPort = *port;
    }

    Logger::instance().info("Starting LanTalk server on port " +
                            std::to_string(config.listenPort));

    ChatSession session(config);

    // Wire up message display callback
    std::unique_ptr<TerminalUI> ui;

    session.setMessageCallback([&ui](const ChatMessage& msg, const std::string& /*name*/) {
        if (ui) {
            ui->printMessage(msg.formatForDisplay());
        }
    });

    session.setPeerEventCallback([&ui](const PeerInfo& peer, bool connected) {
        if (ui) {
            std::string ev = connected ? " connected" : " disconnected";
            ui->printInfo(peer.displayName + "@" + peer.address + ev);
        }
    });

    session.start();

    // Accept callback: wrap each new socket into a Connection and add to session.
    // We use a shared_ptr<string> for connId so the Connection callbacks can
    // reference it after addConnection assigns it.
    TcpServer server(config.listenPort, [&session](std::unique_ptr<TcpSocket> sock) {
        std::string addr = sock->remoteAddress();
        Logger::instance().info("Accepted connection from " + addr);

        // Generate connId here so both callbacks share it
        auto connIdPtr = std::make_shared<std::string>();

        auto conn = std::make_unique<Connection>(
            std::move(sock),
            [&session, connIdPtr](Frame f) {
                session.onFrame(*connIdPtr, std::move(f));
            },
            [&session](const std::string& reason) {
                session.onDisconnect("", reason);
            }
        );
        session.addConnection(std::move(conn));
    });

    if (!server.start()) {
        std::cerr << "\nError: Could not bind to port " << config.listenPort << ".\n"
                  << "Possible causes:\n"
                  << "  - Port is already in use (try --port NNNN)\n"
                  << "  - Insufficient permissions\n";
        return;
    }

    // LAN discovery announce
    LanDiscovery discovery(config);
    if (config.discoveryEnabled) {
        discovery.startAnnounce();
    }

    // Build and run UI
    ui = std::make_unique<TerminalUI>(config, session);
    g_ui_ptr = ui.get();
    setupSignalHandlers();

    ui->printListenBanner("0.0.0.0:" + std::to_string(config.listenPort));
    ui->run(); // blocks until /quit or SIGINT

    // Shutdown
    g_ui_ptr = nullptr;
    discovery.stopAnnounce();
    server.stop();
    session.stop();
}

void runConnect(lantalk::Config& cfg, const lantalk::ParsedArgs& args) {
    using namespace lantalk;
    AppConfig& config = cfg.get();

    if (args.positional.empty()) {
        std::cerr << "Usage: lantalk connect <host|ip> [--port PORT]\n";
        return;
    }

    std::string host = args.positional[0];
    uint16_t port = config.listenPort;
    if (auto p = args.getFlagAsPort("port")) {
        port = *p;
    }

    std::cout << "Connecting to " << host << ":" << port << "...\n";

    // If host looks like a device name/ID (no dots), try scanning first
    bool isIP = (host.find('.') != std::string::npos);
    if (!isIP) {
        std::cout << "Scanning LAN for device \"" << host << "\"...\n";
        LanDiscovery discovery(config);
        auto peers = discovery.scan();
        bool found = false;
        for (const auto& p : peers) {
            if (p.displayName == host || p.deviceId == host) {
                host  = p.ipAddress;
                port  = p.tcpPort;
                found = true;
                std::cout << "Found: " << p.displayName << " at " << host << ":" << port << "\n";
                break;
            }
        }
        if (!found) {
            std::cerr << "\nDevice \"" << args.positional[0]
                      << "\" not found on LAN.\n"
                      << "Try: lantalk scan\n";
            return;
        }
    }

    TcpClient client;
    std::unique_ptr<TcpSocket> sock;
    try {
        sock = client.connect(host, port,
                              std::chrono::seconds{config.connectTimeoutSecs});
    } catch (const ConnectError& e) {
        std::cerr << "\nConnection failed: " << host << ":" << port << "\n"
                  << "Reason: " << e.what() << "\n"
                  << "Possible causes:\n"
                  << "  - LanTalk is not running on the remote device\n"
                  << "  - TCP port " << port << " is blocked by a firewall\n"
                  << "  - IP address is incorrect\n";
        return;
    }

    ChatSession session(config);

    std::unique_ptr<TerminalUI> ui;

    session.setMessageCallback([&ui](const ChatMessage& msg, const std::string& /*name*/) {
        if (ui) {
            ui->printMessage(msg.formatForDisplay());
        }
    });

    session.setPeerEventCallback([&ui](const PeerInfo& peer, bool connected) {
        if (ui) {
            std::string ev = connected ? " connected" : " disconnected";
            ui->printInfo(peer.displayName + "@" + peer.address + ev);
        }
    });

    session.start();

    // Resolve peer name for banner (will be updated after HELLO exchange)
    std::string peerName = host;
    const std::string peerAddr = host + ":" + std::to_string(port);

    auto conn = std::make_unique<Connection>(
        std::move(sock),
        [&session](Frame f) { session.onFrame("", std::move(f)); },
        [&session, &ui](const std::string& reason) {
            session.onDisconnect("", reason);
            if (ui) {
                ui->printInfo("Disconnected: " + reason);
                ui->stop();
            }
        }
    );

    session.addConnection(std::move(conn));

    ui = std::make_unique<TerminalUI>(config, session);
    g_ui_ptr = ui.get();
    setupSignalHandlers();

    ui->printBanner(peerName, peerAddr);
    ui->run();

    g_ui_ptr = nullptr;
    session.stop();
}

void runScan(lantalk::Config& cfg, const lantalk::ParsedArgs& /*args*/) {
    using namespace lantalk;
    const AppConfig& config = cfg.get();

    std::cout << "\nScanning for LanTalk peers on the local network...\n"
              << "(waiting " << LanDiscovery::kScanTimeoutSecs << " seconds)\n\n";

    LanDiscovery discovery(config);
    int count = 0;

    auto peers = discovery.scan([&count](const DiscoveredPeer& p) {
        ++count;
        std::cout << count << ". " << p.displayName << "\n"
                  << "   IP:       " << p.ipAddress   << "\n"
                  << "   Port:     " << p.tcpPort      << "\n"
                  << "   Platform: " << p.platform     << "\n"
                  << "   ID:       " << p.deviceId     << "\n\n";
        std::cout.flush();
    });

    if (peers.empty()) {
        std::cout << "No LanTalk peers found.\n"
                  << "Make sure other devices are running: lantalk listen\n";
    } else {
        std::cout << "Found " << peers.size() << " peer(s).\n"
                  << "Connect with: lantalk connect <ip-or-name>\n";
    }
}

void runConfig(lantalk::Config& cfg, const lantalk::ParsedArgs& args) {
    using namespace lantalk;
    // Sub-commands: show / set KEY VALUE / get KEY
    if (args.positional.empty()) {
        cfg.print();
        return;
    }

    const std::string& sub = args.positional[0];

    if (sub == "show") {
        cfg.print();
    } else if (sub == "set") {
        if (args.positional.size() < 3) {
            std::cerr << "Usage: lantalk config set <key> <value>\n";
            return;
        }
        const std::string& key   = args.positional[1];
        const std::string& value = args.positional[2];
        if (!cfg.set(key, value)) {
            std::cerr << "Unknown config key: " << key << "\n"
                      << "Run 'lantalk config show' to see valid keys.\n";
            return;
        }
        cfg.save();
        std::cout << "Set " << key << " = " << value << "\n"
                  << "Config saved to: " << cfg.configFilePath().string() << "\n";
    } else if (sub == "get") {
        if (args.positional.size() < 2) {
            std::cerr << "Usage: lantalk config get <key>\n";
            return;
        }
        const std::string& key = args.positional[1];
        auto val = cfg.getAsString(key);
        if (!val) {
            std::cerr << "Unknown config key: " << key << "\n";
            return;
        }
        std::cout << key << " = " << *val << "\n";
    } else {
        std::cerr << "Unknown config sub-command: " << sub << "\n"
                  << "Usage: lantalk config [show | set KEY VALUE | get KEY]\n";
    }
}

void runStatus(lantalk::Config& cfg, const lantalk::ParsedArgs& /*args*/) {
    const lantalk::AppConfig& c = cfg.get();
    std::cout << "\nLanTalk Status\n"
              << "==============\n"
              << "Device ID:    " << c.deviceId      << "\n"
              << "Display Name: " << c.displayName   << "\n"
              << "Platform:     " << c.platform      << "\n"
              << "Listen Port:  " << c.listenPort    << "\n"
              << "Discovery:    " << (c.discoveryEnabled ? "enabled" : "disabled") << "\n"
              << "Config file:  " << cfg.configFilePath().string() << "\n";
}

void applyLoggingFlags(const lantalk::ParsedArgs& args) {
    using namespace lantalk;
    if (args.hasFlag("quiet")) {
        Logger::instance().setQuiet(true);
    }
    if (args.hasFlag("verbose")) {
        Logger::instance().setLevel(LogLevel::LevelDebug);
    }
    if (auto lvl = args.getFlag("log-level")) {
        const std::string& s = *lvl;
        if      (s == "trace") Logger::instance().setLevel(LogLevel::LevelTrace);
        else if (s == "debug") Logger::instance().setLevel(LogLevel::LevelDebug);
        else if (s == "info")  Logger::instance().setLevel(LogLevel::LevelInfo);
        else if (s == "warn")  Logger::instance().setLevel(LogLevel::LevelWarn);
        else if (s == "error") Logger::instance().setLevel(LogLevel::LevelError);
        else {
            std::cerr << "Unknown log level: " << s
                      << ". Valid: trace debug info warn error\n";
        }
    }
}

} // anonymous namespace

// ---- Entry point ------------------------------------------------------------

int main(int argc, char* argv[]) {
    // 1. Platform socket initialisation (no-op on POSIX, WSAStartup on Windows)
    if (!lantalk::initializeSockets()) {
        std::cerr << "Fatal: socket initialisation failed.\n";
        return 1;
    }

    // 2. Parse arguments early (we need --help / version before loading config)
    const lantalk::ParsedArgs args = lantalk::CommandParser::parse(argc, argv);

    // 3. Handle top-level flags that don't need config
    if (args.command == "help" || args.hasFlag("help")) {
        lantalk::CommandParser::printHelp();
        lantalk::cleanupSockets();
        return 0;
    }
    if (args.command == "version" || args.hasFlag("version")) {
        lantalk::CommandParser::printVersion();
        lantalk::cleanupSockets();
        return 0;
    }

    // 4. Load / create config
    lantalk::Config cfg;
    cfg.load(); // creates defaults + persists device ID on first run

    // 5. Apply logging flags from CLI
    applyLoggingFlags(args);

    // 6. Dispatch command
    const std::string& cmd = args.command;

    if (cmd == "listen") {
        runListen(cfg, args);
    } else if (cmd == "connect") {
        runConnect(cfg, args);
    } else if (cmd == "scan") {
        runScan(cfg, args);
    } else if (cmd == "config") {
        runConfig(cfg, args);
    } else if (cmd == "status") {
        runStatus(cfg, args);
    } else if (cmd == "peers") {
        // TODO: show peers from a running instance (requires IPC)
        // For now, delegate to scan
        runScan(cfg, args);
    } else if (cmd.empty()) {
        // INTERACTIVE DESKTOP MODE
        // Triggers when user double-clicks the .exe or runs without arguments
        std::cout << R"(
      __               ______      ____ __ 
     / /  ___ ____    /_  __/__ _ / / // /__
    / /__/ _ `/ _ \    / / / _ `// / //  '_/
   /____/\_,_/_//_/   /_/  \_,_//_/_//_/\_\ 
                                           
        ( ( ( ( (  📡  ) ) ) ) )           
                                           
   P2P Local Area Network Messenger v1.0.5  
=============================================
)";
        while (true) {
            std::cout << "\nChoose an action:\n"
                      << "  [1] Listen for incoming connections\n"
                      << "  [2] Scan Wi-Fi/LAN for peers\n"
                      << "  [3] Connect to a specific IP address\n"
                      << "  [4] View Status / Info\n"
                      << "  [5] Exit\n\n"
                      << "LANTALK> ";
                      
            std::string choice;
            if (!std::getline(std::cin, choice)) break;
            
            if (choice == "1") {
                runListen(cfg, args);
                // When they /quit the chat, they return to menu
            } else if (choice == "2") {
                runScan(cfg, args);
            } else if (choice == "3") {
                std::cout << "Enter IP Address (e.g. 192.168.1.5): ";
                std::string ip;
                if (std::getline(std::cin, ip) && !ip.empty()) {
                    lantalk::ParsedArgs connectArgs = args;
                    connectArgs.positional.push_back(ip);
                    runConnect(cfg, connectArgs);
                }
            } else if (choice == "4") {
                runStatus(cfg, args);
            } else if (choice == "5" || choice == "exit" || choice == "quit") {
                break;
            } else if (!choice.empty()) {
                std::cout << "Invalid choice. Enter 1-5.\n";
            }
        }
    } else {
        std::cerr << "Unknown command: " << cmd << "\n"
                  << "Run 'lantalk --help' for usage.\n";
        lantalk::cleanupSockets();
        return 1;
    }

    // 7. Cleanup
    lantalk::cleanupSockets();
    return 0;
}
