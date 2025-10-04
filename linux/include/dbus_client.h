#pragma once
#include <string>
#include <vector>

#include <functional>

namespace openlawsvpn {

class DBusClient {
public:
    DBusClient();
    ~DBusClient();

    typedef std::function<void(const std::string&)> LogCallback;
    void set_log_callback(LogCallback callback);

    void connect(const std::string& config_file, 
                 const std::string& state_id, 
                 const std::string& token,
                 const std::string& remote_ip = "");
    void disconnect();

    void set_log_level(int level);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace openlawsvpn
