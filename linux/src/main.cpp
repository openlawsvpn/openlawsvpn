#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <csignal>
#include "libopenlawsvpn.h"
#include "saml_capture.h"

namespace {
    openlawsvpn::OpenVPNClient* g_client = nullptr;
}

static void signal_handler(int sig) {
    if (g_client) {
        std::cout << "\n" << openlawsvpn::get_log_prefix() << " Caught signal " << sig << ", disconnecting..." << std::endl;
        g_client->disconnect();
    }
    std::exit(sig);
}

static void open_browser(const std::string& url) {
    if (url.empty()) {
        std::cerr << openlawsvpn::get_log_prefix() << " Error: SAML URL is empty." << std::endl;
        return;
    }

    // Ensure the URL has a scheme
    std::string full_url = url;
    if (full_url.find("://") == std::string::npos) {
        full_url = "https://" + full_url;
    }

    std::cout << openlawsvpn::get_log_prefix() << " Attempting to open browser for SAML login..." << std::endl;
    
    // Use xdg-open via std::system as it's the standard way on Linux to open URLs
    // without depending on a specific desktop environment's libraries.
    std::string cmd = "xdg-open '" + full_url + "' >/dev/null 2>&1 &";
    int status = std::system(cmd.c_str());
    
    if (status == 0) {
        std::cout << openlawsvpn::get_log_prefix() << " Browser opening command dispatched." << std::endl;
    } else {
        std::cerr << openlawsvpn::get_log_prefix() << " Failed to execute browser opening command (exit code: " << status << ")." << std::endl;
        std::cerr << openlawsvpn::get_log_prefix() << " Please open the URL manually: " << full_url << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " connect <config.ovpn>" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    if (command == "connect") {
        if (argc < 3) {
            std::cerr << "Error: config file required" << std::endl;
            return 1;
        }
        std::string config_file = argv[2];
        
        try {
            openlawsvpn::OpenVPNClient client(config_file);
            g_client = &client;
            
            // Register signal handlers
            std::signal(SIGINT, signal_handler);
            std::signal(SIGTERM, signal_handler);
            
            // Handle options
            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--log-level" || arg == "-l") {
                    if (i + 1 < argc) {
                        client.set_log_level(std::stoi(argv[i+1]));
                    }
                } else if (arg == "--standalone") {
                    client.set_connect_mode(openlawsvpn::ConnectMode::DIRECT);
                }
            }

            std::cout << openlawsvpn::get_log_prefix() << " Phase 1: connecting to provoke CRV1 challenge...\n" << std::flush;
            auto phase1 = client.connect_phase1();
            
            std::cout << openlawsvpn::get_log_prefix() << " SAML URL received: " << phase1.saml_url << "\n" << std::flush;
            std::cout << openlawsvpn::get_log_prefix() << " State ID: " << phase1.state_id << "\n" << std::flush;
            std::cout << openlawsvpn::get_log_prefix() << " Please complete SAML login in your browser.\n" << std::flush;
            
            openlawsvpn::SAMLCapture capture;
            auto token_future = capture.start();
            
            // In a real CLI we might want to automatically open the browser
            open_browser(phase1.saml_url);

            std::string token = token_future.get();
            std::cout << openlawsvpn::get_log_prefix() << " SAML token received.\n" << std::flush;

            std::cout << openlawsvpn::get_log_prefix() << " Phase 2: establishing tunnel...\n" << std::flush;
            client.connect_phase2(phase1.state_id, token, phase1.remote_ip);
            
            std::cout << openlawsvpn::get_log_prefix() << " Tunnel is up.\n" << std::flush;
            std::cout << openlawsvpn::get_log_prefix() << " Connected. Press Ctrl-C to disconnect.\n" << std::flush;
            
            while(true) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
