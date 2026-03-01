// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 openlawsvpn contributors
// See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
#ifdef ENABLE_DBUS
#include "dbus_client.h"
#include "libopenlawsvpn.h"
#include <gio/gio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

namespace openlawsvpn {

struct DBusClient::Impl {
    GDBusConnection* conn = nullptr;
    std::string config_path;
    std::string session_path;
    int log_level = 1;
    unsigned int signal_id = 0;
    std::mutex mtx;
    std::condition_variable cv;
    bool connected = false;
    bool failed = false;
    std::string error_msg;
    LogCallback log_callback;

    void emit_log(const std::string& msg) {
        if (log_callback) {
            log_callback(msg);
        } else {
            std::cout << msg << std::flush;
        }
    }

    Impl() {}
    ~Impl() {
        if (signal_id && conn) {
            g_dbus_connection_signal_unsubscribe(conn, signal_id);
        }
        if (conn) g_object_unref(conn);
    }

    static void on_status_change(GDBusConnection* connection,
                                 const gchar* sender_name,
                                 const gchar* object_path,
                                 const gchar* interface_name,
                                 const gchar* signal_name,
                                 GVariant* parameters,
                                 gpointer user_data) {
        Impl* impl = static_cast<Impl*>(user_data);

        if (signal_name && (g_str_equal(signal_name, "StatusChange") || g_str_equal(signal_name, "Status"))) {
            uint32_t major = 0, minor = 0;
            const gchar* message = nullptr;
            
            if (g_variant_is_of_type(parameters, G_VARIANT_TYPE("(uus)"))) {
                g_variant_get(parameters, "(uus)", &major, &minor, &message);
            } else {
                return;
            }

            if (impl->log_level >= 1) {
                std::string atom = get_log_prefix() + " D-Bus StatusChange: (" + std::to_string(major) + ", " + std::to_string(minor) + ") " + (message ? message : "") + "\n";
                impl->emit_log(atom);
            }

            std::lock_guard<std::mutex> lock(impl->mtx);
            if (major == 2) {
                if (minor == 101 || (message && (strstr(message, "Connected") || strstr(message, "CONNECTED")))) {
                    impl->connected = true;
                    impl->cv.notify_all();
                } else if (minor == 103 || minor == 110) {
                    if (!impl->connected) {
                        impl->failed = true;
                        impl->error_msg = message ? message : "Connection finished without connecting";
                        impl->cv.notify_all();
                    } else {
                        impl->emit_log(get_log_prefix() + " VPN Disconnected (D-Bus StatusChange)\n");
                    }
                } else if (minor == 104 || minor == 105 || (message && (strstr(message, "failed") || strstr(message, "FAILED")))) {
                    impl->failed = true;
                    impl->error_msg = message ? message : "Connection failed";
                    impl->cv.notify_all();
                }
            } else if (major == 3) {
                if (minor == 113) {
                    if (!impl->connected) {
                        impl->failed = true;
                        impl->error_msg = message ? message : "Backend session completed unexpectedly";
                        impl->cv.notify_all();
                    }
                }
            }
        } else if (signal_name && g_str_equal(signal_name, "Log")) {
            uint32_t type = 0, group = 0;
            const gchar* tag = nullptr;
            const gchar* message = nullptr;
            
            if (g_variant_is_of_type(parameters, G_VARIANT_TYPE("(uuss)"))) {
                g_variant_get(parameters, "(uuss)", &type, &group, &tag, &message);
                
                if (impl->log_level >= 1) {
                    std::string atom = get_log_prefix() + " D-Bus LOG: " + std::string(message ? message : "") + "\n";
                    impl->emit_log(atom);
                }
                
                if (message && (strstr(message, "Connected") || strstr(message, "CONNECTED"))) {
                    std::lock_guard<std::mutex> lock(impl->mtx);
                    impl->connected = true;
                    impl->cv.notify_all();
                }
            }
        }
    }
};

DBusClient::DBusClient() : impl_(new Impl()) {
    GError* error = nullptr;
    impl_->conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!impl_->conn) {
        std::string msg = "Failed to connect to system bus: ";
        msg += error->message;
        g_error_free(error);
        throw std::runtime_error(msg);
    }
}

DBusClient::~DBusClient() {
    delete impl_;
}

void DBusClient::set_log_level(int level) {
    impl_->log_level = level;
}

void DBusClient::set_log_callback(LogCallback callback) {
    impl_->log_callback = callback;
}

void DBusClient::connect(const std::string& config_file, 
                         const std::string& state_id, 
                         const std::string& token,
                         const std::string& remote_ip) {
    GError* error = nullptr;
    
    // 1. Import config
    std::string config_content = OpenVPNClient::read_and_filter_config(config_file);
    
    if (!remote_ip.empty()) {
        if (impl_->log_level >= 1) {
            std::cout << get_log_prefix() << " Using sticky IP for D-Bus: " << remote_ip << std::endl;
        }
        config_content = "remote " + remote_ip + " 443\n" + config_content;
    }

    GVariant* res = nullptr;
    for (int retry = 0; retry < 5; ++retry) {
        if (error) {
            g_error_free(error);
            error = nullptr;
        }
        res = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.configuration",
            "/net/openvpn/v3/configuration",
            "net.openvpn.v3.configuration",
            "Import",
            g_variant_new("(ssbb)", "openlawsvpn", config_content.c_str(), true, false),
            G_VARIANT_TYPE("(o)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1, nullptr, &error
        );
        if (res) break;

        if (g_dbus_error_is_remote_error(error) && 
            (g_str_equal(g_dbus_error_get_remote_error(error), "org.freedesktop.DBus.Error.ServiceUnknown") ||
             g_str_equal(g_dbus_error_get_remote_error(error), "org.freedesktop.DBus.Error.UnknownMethod") ||
             g_str_equal(g_dbus_error_get_remote_error(error), "org.freedesktop.DBus.Error.FileNotFound"))) {
            if (impl_->log_level >= 1) {
                std::cout << get_log_prefix() << " Config manager not ready, retrying... (" << retry + 1 << "/5)" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        break;
    }

    if (!res) {
        std::string msg = "Failed to import configuration: ";
        msg += error->message;
        g_error_free(error);
        throw std::runtime_error(msg);
    }

    const char* config_obj_path;
    g_variant_get(res, "(&o)", &config_obj_path);
    impl_->config_path = config_obj_path;
    g_variant_unref(res);

    if (impl_->log_level >= 1) {
        std::cout << get_log_prefix() << " Config imported: " << impl_->config_path << std::endl;
    }

    // 2. New Tunnel
    for (int retry = 0; retry < 5; ++retry) {
        if (error) {
            g_error_free(error);
            error = nullptr;
        }
        res = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.sessions",
            "/net/openvpn/v3/sessions",
            "net.openvpn.v3.sessions",
            "NewTunnel",
            g_variant_new("(o)", impl_->config_path.c_str()),
            G_VARIANT_TYPE("(o)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1, nullptr, &error
        );
        if (res) break;
        
        if (g_dbus_error_is_remote_error(error) && 
            (g_str_equal(g_dbus_error_get_remote_error(error), "org.freedesktop.DBus.Error.ServiceUnknown") ||
             g_str_equal(g_dbus_error_get_remote_error(error), "org.freedesktop.DBus.Error.UnknownMethod"))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        std::string msg = "Failed to create new tunnel: ";
        msg += error->message;
        g_error_free(error);
        throw std::runtime_error(msg);
    }

    const char* session_obj_path;
    g_variant_get(res, "(&o)", &session_obj_path);
    impl_->session_path = session_obj_path;
    g_variant_unref(res);

    if (impl_->log_level >= 1) {
        std::cout << get_log_prefix() << " Session path received: " << impl_->session_path << std::endl;
    }

    // 3. Synchronization loop: wait for session object to appear on D-Bus
    bool session_ready = false;
    for (int i = 0; i < 20; ++i) {
        GVariant* introspect = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.sessions",
            impl_->session_path.c_str(),
            "org.freedesktop.DBus.Introspectable",
            "Introspect",
            nullptr,
            G_VARIANT_TYPE("(s)"),
            G_DBUS_CALL_FLAGS_NONE,
            500, nullptr, nullptr
        );
        if (introspect) {
            g_variant_unref(introspect);
            session_ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    if (!session_ready) {
        throw std::runtime_error("Timeout waiting for session object to become available on D-Bus");
    }

    // 4. Provide credentials (CRV1 response)
    // Username N/A
    // Password CRV1:R:state_id:token
    // According to docs, type=1 (CREDENTIALS), group=1 (USER_PASSWORD)
    std::string password = "CRV1::" + state_id + "::" + token;
    const char* pwd_ptr = password.c_str();

    auto provide_credential = [&](uint32_t type, uint32_t group, uint32_t id, const char* value) {
        GError* inner_err = nullptr;
        if (impl_->log_level >= 2) {
            std::cout << get_log_prefix() << " Providing credential: type=" << type << ", group=" << group << ", id=" << id << std::endl;
        }
        GVariant* ui_res = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.sessions",
            impl_->session_path.c_str(),
            "net.openvpn.v3.sessions",
            "UserInputProvide",
            g_variant_new("(uuus)", type, group, id, value),
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1, nullptr, &inner_err
        );
        if (ui_res) {
            g_variant_unref(ui_res);
        } else {
            if (impl_->log_level >= 2) std::cout << get_log_prefix() << " Note: Failed to provide credential (" << id << "): " << inner_err->message << std::endl;
            g_error_free(inner_err);
        }
    };

    // Ready/Connect loop
    bool is_ready = false;
    for (int loop = 0; loop < 20; ++loop) {
        // Provide standard credentials before calling Ready()
        // type=1 (CREDENTIALS), group=1 (USER_PASSWORD), id=0 (username), id=1 (password)
        provide_credential(1, 1, 0, "N/A");
        provide_credential(1, 1, 1, pwd_ptr);

        GError* r_err = nullptr;
        GVariant* r_res = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.sessions",
            impl_->session_path.c_str(),
            "net.openvpn.v3.sessions",
            "Ready",
            nullptr,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1, nullptr, &r_err
        );

        if (r_res) {
            g_variant_unref(r_res);
            is_ready = true;
            if (impl_->log_level >= 1) std::cout << get_log_prefix() << " Session is ready after " << loop << " retries." << std::endl;
            break;
        } else {
            // Check if it's just missing credentials, in which case we continue the loop
            bool missing_creds = false;
            if (g_dbus_error_is_remote_error(r_err)) {
                const char* remote_err = g_dbus_error_get_remote_error(r_err);
                if (g_str_has_suffix(remote_err, "MissingCredentials") || 
                    g_str_has_suffix(remote_err, "ready.Code36") ||
                    strstr(r_err->message, "Missing user credentials")) {
                    missing_creds = true;
                }
            }

            if (impl_->log_level >= 1 && (!missing_creds || loop % 5 == 0)) {
                std::cout << get_log_prefix() << " Ready() attempt " << loop << " failed: " << r_err->message << std::endl;
            }
            g_error_free(r_err);
            
            if (!missing_creds && loop > 5) {
                // If it's a different error, maybe we should stop early, but let's be persistent for now
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (!is_ready) {
        throw std::runtime_error("Session not ready for connection");
    }

    // 5. Connect
    if (impl_->log_level >= 1) {
        impl_->emit_log(get_log_prefix() + " Calling Connect() via D-Bus...\n");
    }

    // Subscribe to status changes BEFORE calling Connect
    impl_->signal_id = g_dbus_connection_signal_subscribe(
        impl_->conn,
        "net.openvpn.v3.sessions",
        "net.openvpn.v3.sessions",
        nullptr, // Any member (StatusChange or Log)
        impl_->session_path.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        Impl::on_status_change,
        impl_,
        nullptr
    );

    // Call LogForward(true) to ensure the backend forwards status/log events
    {
        GError* lf_err = nullptr;
        GVariant* lf_res = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.sessions",
            impl_->session_path.c_str(),
            "net.openvpn.v3.sessions",
            "LogForward",
            g_variant_new("(b)", true),
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1, nullptr, &lf_err
        );
        if (lf_res) {
            g_variant_unref(lf_res);
        } else {
            if (impl_->log_level >= 2) std::cout << get_log_prefix() << " Note: LogForward(true) failed: " << lf_err->message << std::endl;
            g_error_free(lf_err);
        }
    }

    res = g_dbus_connection_call_sync(
        impl_->conn,
        "net.openvpn.v3.sessions",
        impl_->session_path.c_str(),
        "net.openvpn.v3.sessions",
        "Connect",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1, nullptr, &error
    );

    if (!res) {
        std::string msg = "Failed to initiate connect via D-Bus: ";
        msg += error->message;
        g_error_free(error);
        throw std::runtime_error(msg);
    }
    g_variant_unref(res);

    // Wait for CONNECTED or FAILURE signal
    if (impl_->log_level >= 1) {
        impl_->emit_log(get_log_prefix() + " Waiting for connection to be established...\n");
    }

    auto start_wait = std::chrono::steady_clock::now();
    while (true) {
        // Process D-Bus signals
        while (g_main_context_iteration(nullptr, FALSE)) {
            // Signals are handled in callbacks
        }

        {
            std::unique_lock<std::mutex> lock(impl_->mtx);
            bool signaled = impl_->cv.wait_for(lock, std::chrono::milliseconds(500), [this] {
                return impl_->connected || impl_->failed;
            });

            if (signaled) {
                break;
            }
        }

        // Fallback: Poll the "status" property of the session object
        GError* p_err = nullptr;
        GVariant* p_val = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.sessions",
            impl_->session_path.c_str(),
            "org.freedesktop.DBus.Properties",
            "Get",
            g_variant_new("(ss)", "net.openvpn.v3.sessions", "status"),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE,
            1000, nullptr, &p_err
        );

        if (p_val) {
            GVariant* inner = nullptr;
            g_variant_get(p_val, "(v)", &inner);
            if (inner) {
                uint32_t major, minor;
                const gchar* message;
                if (g_variant_is_of_type(inner, G_VARIANT_TYPE("(uus)"))) {
                    g_variant_get(inner, "(uus)", &major, &minor, &message);
                    if (impl_->log_level >= 2) {
                        std::string atom = get_log_prefix() + " Polled Status: (" + std::to_string(major) + ", " + std::to_string(minor) + ") " + (message ? message : "") + "\n";
                        impl_->emit_log(atom);
                    }
                    
                    std::lock_guard<std::mutex> lock(impl_->mtx);
                    if (major == 2 && (minor == 101 || (message && (strstr(message, "Connected") || strstr(message, "CONNECTED"))))) {
                        impl_->connected = true;
                        impl_->cv.notify_all();
                        g_variant_unref(inner);
                        g_variant_unref(p_val);
                        break;
                    } else if (major == 2 && (minor == 104 || minor == 105)) {
                        impl_->failed = true;
                        impl_->error_msg = message ? message : "Connection failed (polled)";
                        impl_->cv.notify_all();
                        g_variant_unref(inner);
                        g_variant_unref(p_val);
                        break;
                    }
                }
                g_variant_unref(inner);
            }
            g_variant_unref(p_val);
        } else {
            g_error_free(p_err);
        }

        // Also check "connected_to" property
        GError* c_err = nullptr;
        GVariant* c_val = g_dbus_connection_call_sync(
            impl_->conn,
            "net.openvpn.v3.sessions",
            impl_->session_path.c_str(),
            "org.freedesktop.DBus.Properties",
            "Get",
            g_variant_new("(ss)", "net.openvpn.v3.sessions", "connected_to"),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE,
            1000, nullptr, &c_err
        );

        if (c_val) {
            GVariant* inner = nullptr;
            g_variant_get(c_val, "(v)", &inner);
            if (inner) {
                const gchar *remote_host, *remote_port, *remote_proto;
                if (g_variant_is_of_type(inner, G_VARIANT_TYPE("(ssu)"))) {
                    g_variant_get(inner, "(ssu)", &remote_host, &remote_port, &remote_proto);
                    if (remote_host && strlen(remote_host) > 0) {
                        if (impl_->log_level >= 1) {
                            std::string atom = get_log_prefix() + " Connected to " + std::string(remote_host) + " (polled connected_to)\n";
                            impl_->emit_log(atom);
                        }
                        std::lock_guard<std::mutex> lock(impl_->mtx);
                        impl_->connected = true;
                        impl_->cv.notify_all();
                        g_variant_unref(inner);
                        g_variant_unref(c_val);
                        break;
                    }
                }
                g_variant_unref(inner);
            }
            g_variant_unref(c_val);
        } else {
            g_error_free(c_err);
        }

        auto elapsed = std::chrono::steady_clock::now() - start_wait;
        if (elapsed > std::chrono::seconds(90)) {
            if (!impl_->session_path.empty()) {
                disconnect();
            }
            throw std::runtime_error("Timeout waiting for D-Bus connection status");
        }
    }

    if (impl_->failed) {
        if (!impl_->session_path.empty()) {
            disconnect();
        }
        throw std::runtime_error("VPN connection failed: " + impl_->error_msg);
    }

    if (impl_->log_level >= 1) {
        std::cout << get_log_prefix() << " Tunnel is up." << std::endl;
    }
}

void DBusClient::disconnect() {
    if (impl_->session_path.empty()) return;
    GError* error = nullptr;
    GVariant* res = g_dbus_connection_call_sync(
        impl_->conn,
        "net.openvpn.v3.sessions",
        impl_->session_path.c_str(),
        "net.openvpn.v3.sessions",
        "Disconnect",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1, nullptr, &error
    );
    if (res) g_variant_unref(res);
    else if (error) g_error_free(error);
}

} // namespace openlawsvpn
#endif // ENABLE_DBUS
