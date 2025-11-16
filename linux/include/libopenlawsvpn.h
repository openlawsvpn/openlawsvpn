#pragma once
#include <string>
#include <functional>

namespace openlawsvpn {

enum class ConnectMode {
    DIRECT,
    DBUS
};

typedef void (*LogCallback)(const char* message, void* user_data);

class OpenVPNClient {
public:
    OpenVPNClient(const std::string& config_path);
    ~OpenVPNClient();

    void set_connect_mode(ConnectMode mode);
    void set_log_callback(LogCallback callback, void* user_data);

    // Connect to trigger Phase 1 (get SAML URL)
    struct Phase1Result {
        std::string saml_url;
        std::string state_id;
        std::string remote_ip;
    };
    Phase1Result connect_phase1();
    void set_log_level(int level);

    // Connect with captured SAML token for Phase 2
    void connect_phase2(const std::string& state_id, const std::string& token, const std::string& remote_ip = "");

    void disconnect();

    // Read and filter config file (removes AWS-proprietary directives)
    static std::string read_and_filter_config(const std::string& path);

private:
    std::string config_path_;
    ConnectMode mode_;
    LogCallback log_callback_ = nullptr;
    void* user_data_ = nullptr;
    struct Impl;
    Impl* impl_;

    void emit_log(const std::string& msg);
public:
    void emit_log_public(const std::string& msg) { emit_log(msg); }
};

std::string get_log_prefix();

} // namespace openlawsvpn
