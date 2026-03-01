// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 openlawsvpn contributors
// See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
/*
 * openlawsvpn — A specialized OpenVPN 3 client for Linux.
 * Copyright (C) 2026 Anatolii Vorona <vorona.tolik@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "libopenlawsvpn.h"
#ifdef ENABLE_DBUS
#include "dbus_client.h"
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <cstdio>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>

// OpenVPN 3 Core headers
#ifndef OPENVPN_CORE_API_VISIBILITY_HIDDEN
#define OPENVPN_CORE_API_VISIBILITY_HIDDEN
#endif

#ifndef USE_TUN_BUILDER
#define USE_TUN_BUILDER
#endif

#include <client/ovpncli.cpp>
#include <openvpn/tun/builder/capture.hpp>
#include <openvpn/tun/linux/client/tunmethods.hpp>
#include <openvpn/tun/linux/client/tunsetup.hpp>

namespace {
    int g_log_level = 1;
}

using namespace openvpn;

namespace openlawsvpn {

std::string get_log_prefix() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt;
    localtime_r(&in_time_t, &bt);
    char buf[64];
    // User requested [openlawsvpn YYYY-MM-DD MM:ss]
    // but likely meant HH:mm:ss. I will use %Y-%m-%d %H:%M:%S as it's more standard,
    // but I'll try to follow "YYYY-MM-DD MM:ss" if it's literal.
    // "MM:ss" usually means minutes:seconds.
    // Let's use %Y-%m-%d %H:%M:%S.
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &bt);
    return "[openlawsvpn " + std::string(buf) + "]";
}

class OpenVPNClient;

static openvpn::InitProcess::Init g_init_process;

class CoreClient : public openvpn::ClientAPI::OpenVPNClient {
public:
    CoreClient(class openlawsvpn::OpenVPNClient* parent) : parent_(parent), saml_url_(""), state_id_(""), remote_ip_(""), done_(false), connected_(false), error_(false), error_msg_("") {}

    // TunBuilder methods
    virtual bool tun_builder_new() override {
        capture_.reset(new openvpn::TunBuilderCapture());
        return true;
    }

    virtual bool tun_builder_set_remote_address(const std::string& address, bool ipv6) override {
        return capture_->tun_builder_set_remote_address(address, ipv6);
    }

    virtual bool tun_builder_add_address(const std::string& address, int prefix_length, const std::string& gateway, bool ipv6, bool net30) override {
        return capture_->tun_builder_add_address(address, prefix_length, gateway, ipv6, net30);
    }

    virtual bool tun_builder_set_mtu(int mtu) override {
        return capture_->tun_builder_set_mtu(mtu);
    }

    virtual bool tun_builder_set_dns_options(const openvpn::DnsOptions& dns) override {
        return capture_->tun_builder_set_dns_options(dns);
    }

    virtual bool tun_builder_add_route(const std::string& address, int prefix_length, int metric, bool ipv6) override {
        return capture_->tun_builder_add_route(address, prefix_length, metric, ipv6);
    }

    virtual bool tun_builder_reroute_gw(bool ipv4, bool ipv6, unsigned int flags) override {
        return capture_->tun_builder_reroute_gw(ipv4, ipv6, flags);
    }

    virtual bool tun_builder_exclude_route(const std::string& address, int prefix_length, int metric, bool ipv6) override {
        return capture_->tun_builder_exclude_route(address, prefix_length, metric, ipv6);
    }

    virtual bool tun_builder_add_proxy_bypass(const std::string& host) override {
        return capture_->tun_builder_add_proxy_bypass(host);
    }

    virtual bool tun_builder_set_proxy_auto_config_url(const std::string& url) override {
        return capture_->tun_builder_set_proxy_auto_config_url(url);
    }

    virtual bool tun_builder_set_proxy_http(const std::string& host, int port) override {
        return capture_->tun_builder_set_proxy_http(host, port);
    }

    virtual bool tun_builder_set_proxy_https(const std::string& host, int port) override {
        return capture_->tun_builder_set_proxy_https(host, port);
    }

    virtual bool tun_builder_set_session_name(const std::string& name) override {
        return capture_->tun_builder_set_session_name(name);
    }

    virtual int tun_builder_establish() override;

    virtual bool tun_builder_set_layer(int layer) override {
        return capture_->tun_builder_set_layer(layer);
    }

    virtual bool socket_protect(int, std::string remote, bool) override {
        if (g_log_level > 1) {
            std::cout << get_log_prefix() << " Socket protect: " << remote << std::endl;
        }
        remote_ip_ = remote;
        return true;
    }

    virtual void event(const openvpn::ClientAPI::Event& ev) override;

    void log(const openvpn::ClientAPI::LogInfo& l) override {
        if (g_log_level > 0) {
            // Filter out verbose transport logs at default info level (1)
            if (g_log_level == 1 && (l.text.find("Transport SEND") != std::string::npos || 
                                     l.text.find("Transport RECV") != std::string::npos)) {
                return;
            }
            
            // Filter out binary data that looks like ASN.1 DER (common in cert/OCSP logs)
            // ASN.1 DER usually starts with 0x30 (SEQUENCE). 
            // If the message contains non-printable characters or starts with 0x30 followed by non-printable, skip it.
            const std::string& text = l.text;
            int non_printable = 0;
            for (size_t i = 0; i < std::min(text.length(), (size_t)100); ++i) {
                unsigned char c = (unsigned char)text[i];
                if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c > 126) {
                    non_printable++;
                }
            }
            if (non_printable > 5) return; // Heuristic: more than 5 non-printable in first 100 chars

            std::string sanitized = text;
            while (!sanitized.empty() && (sanitized.back() == '\n' || sanitized.back() == '\r')) sanitized.pop_back();
            std::string output = get_log_prefix() + " LOG: " + sanitized + "\n";
            parent_->emit_log_public(output);
        }
    }

    virtual bool pause_on_connection_timeout() override {
        return false;
    }

    virtual void acc_event(const openvpn::ClientAPI::AppCustomControlMessageEvent&) override {}
    virtual void external_pki_cert_request(openvpn::ClientAPI::ExternalPKICertRequest&) override {}
    virtual void external_pki_sign_request(openvpn::ClientAPI::ExternalPKISignRequest&) override {}

    struct Challenge {
        std::string saml_url;
        std::string state_id;
        std::string remote_ip;
    };

    Challenge wait_for_saml() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, std::chrono::seconds(30), [this]{ return done_ || error_; })) {
             throw std::runtime_error("Timeout waiting for SAML challenge");
        }
        if (error_) throw std::runtime_error(error_msg_);
        return {saml_url_, state_id_, remote_ip_};
    }
    
    void wait_for_connected() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, std::chrono::seconds(30), [this]{ return connected_ || error_; })) {
             throw std::runtime_error("Timeout waiting for connection");
        }
        if (error_) throw std::runtime_error(error_msg_);
    }

private:
    openlawsvpn::OpenVPNClient* parent_;
    std::string saml_url_;
    std::string state_id_;
    std::string remote_ip_;
    bool done_;
    bool connected_;
    bool error_;
    std::string error_msg_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::unique_ptr<openvpn::TunBuilderCapture> capture_;
};

struct OpenVPNClient::Impl {
    std::shared_ptr<CoreClient> core_client;
#ifdef ENABLE_DBUS
    std::shared_ptr<DBusClient> dbus_client;
#endif
};

void OpenVPNClient::emit_log(const std::string& msg) {
    if (log_callback_) {
        log_callback_(msg.c_str(), user_data_);
    } else {
        std::cout << msg << std::flush;
    }
}

int CoreClient::tun_builder_establish() {
    try {
        openvpn::TunLinuxSetup::Setup<openvpn::TunIPRoute::TunMethods> setup;
        openvpn::TunLinuxSetup::Setup<openvpn::TunIPRoute::TunMethods>::Config config;
        config.layer = openvpn::Layer(openvpn::Layer::OSI_LAYER_3);
        std::cout << get_log_prefix() << " Establishing TUN interface..." << std::endl;
        parent_->emit_log_public(get_log_prefix() + " Establishing TUN interface...\n");
        int fd = setup.establish(*capture_, &config, nullptr, std::cout);
        if (fd >= 0) {
            std::cout << get_log_prefix() << " TUN interface established, fd=" << fd << std::endl;
            parent_->emit_log_public(get_log_prefix() + " TUN interface established, fd=" + std::to_string(fd) + "\n");
        } else {
            std::cerr << get_log_prefix() << " Failed to establish TUN interface" << std::endl;
            parent_->emit_log_public(get_log_prefix() + " Failed to establish TUN interface\n");
        }
        return fd;
    } catch (const std::exception& e) {
        std::cerr << get_log_prefix() << " TunBuilder establish error: " << e.what() << std::endl;
        parent_->emit_log_public(get_log_prefix() + " TunBuilder establish error: " + std::string(e.what()) + "\n");
        return -1;
    }
}

void CoreClient::event(const openvpn::ClientAPI::Event& ev) {
    if (g_log_level > 0) {
        std::string text = ev.info;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        std::string output = get_log_prefix() + " EVENT: " + ev.name + ": " + text + "\n";
        parent_->emit_log_public(output);
    }
    if (ev.name == "DYNAMIC_CHALLENGE") {
            // CRV1 format: CRV1:<flags>:<state_id>:<base64_username>:<challenge_text>
            // For SAML, challenge_text is the URL.
            std::string info = ev.info;
            if (info.substr(0, 5) == "CRV1:") {
                std::vector<std::string> parts;
                size_t last = 5;
                size_t next = 0;
                // Split the first 3 parts (flags, state_id, base64_username)
                for (int i = 0; i < 3; ++i) {
                    next = info.find(':', last);
                    if (next == std::string::npos) break;
                    parts.push_back(info.substr(last, next - last));
                    last = next + 1;
                }
                // The rest is the challenge text (SAML URL)
                parts.push_back(info.substr(last));

                if (parts.size() >= 4) {
                    state_id_ = parts[1];
                    saml_url_ = parts[3];

                std::unique_lock<std::mutex> lock(mutex_);
                done_ = true;
                cv_.notify_all();
                
                // Stop connection after receiving challenge
                stop();
            }
        }
    } else if (ev.name == "CONNECTED") {
         std::unique_lock<std::mutex> lock(mutex_);
         connected_ = true;
         cv_.notify_all();
    } else if (ev.name == "FATAL_ERROR" || ev.name == "AUTH_FAILED") {
         std::unique_lock<std::mutex> lock(mutex_);
         error_ = true;
         error_msg_ = ev.name + ": " + ev.info;
         cv_.notify_all();
    }
}


// CoreClient::log moved inline to class definition

OpenVPNClient::OpenVPNClient(const std::string& config_path) 
    : config_path_(config_path), impl_(new Impl()) {
#ifdef ENABLE_DBUS
    mode_ = ConnectMode::DBUS;
#else
    mode_ = ConnectMode::DIRECT;
#endif
}

OpenVPNClient::~OpenVPNClient() {
    delete impl_;
}

void OpenVPNClient::set_connect_mode(ConnectMode mode) {
    mode_ = mode;
}

void OpenVPNClient::set_log_level(int level) {
    g_log_level = level;
}

std::string OpenVPNClient::read_and_filter_config(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) throw std::runtime_error("Cannot open config file: " + path);
    
    std::string line;
    std::stringstream ss;
    while (std::getline(ifs, line)) {
        // Remove trailing \r if any
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.find("auth-federate") != std::string::npos ||
            line.find("auth-retry") != std::string::npos ||
            line.find("resolv-retry") != std::string::npos) {
            continue;
        }
        ss << line << "\n";
    }
    return ss.str();
}

void OpenVPNClient::set_log_callback(LogCallback callback, void* user_data) {
    log_callback_ = callback;
    user_data_ = user_data;
}

OpenVPNClient::Phase1Result OpenVPNClient::connect_phase1() {
    // Phase 1 is always DIRECT for now because it just needs a challenge and doesn't create a TUN.
    // It's much simpler than going through D-Bus for a connection that is supposed to fail.
    impl_->core_client = std::make_shared<CoreClient>(this);
    ClientAPI::Config config;
    
    config.content = read_and_filter_config(config_path_);
    config.disableClientCert = true;
    config.sslDebugLevel = 0;
    
    ClientAPI::EvalConfig eval = impl_->core_client->eval_config(config);
    if (eval.error) throw std::runtime_error("Config evaluation failed: " + eval.message);

    ClientAPI::ProvideCreds creds;
    creds.username = "N/A";
    creds.password = "ACS::35001";
    impl_->core_client->provide_creds(creds);

    std::cout << get_log_prefix() << " Starting Phase 1 connect..." << std::endl;
    // Connect in a thread to allow wait_for_saml to work if connect() blocks indefinitely
    std::thread p1_thread([this]() {
        try {
            impl_->core_client->connect();
        } catch (...) {}
    });
    p1_thread.detach();
    
    auto result = impl_->core_client->wait_for_saml();
    return {result.saml_url, result.state_id, result.remote_ip};
}

void OpenVPNClient::connect_phase2(const std::string& state_id, const std::string& token, const std::string& remote_ip) {
    if (mode_ == ConnectMode::DIRECT) {
        impl_->core_client = std::make_shared<CoreClient>(this);
        ClientAPI::Config config;
        
        config.content = read_and_filter_config(config_path_);
        config.disableClientCert = true;
        config.sslDebugLevel = 0;
        
        if (!remote_ip.empty()) {
            std::cout << get_log_prefix() << " Forcing remote IP: " << remote_ip << std::endl;
            config.serverOverride = remote_ip;
        }
        
        impl_->core_client->eval_config(config);
        
        ClientAPI::ProvideCreds creds;
        creds.username = "N/A";
        creds.password = "CRV1::" + state_id + "::" + token;
        std::cout << get_log_prefix() << " Total password length: " << creds.password.length() << std::endl;
        std::cout << get_log_prefix() << " SAML token length: " << token.length() << std::endl;
        impl_->core_client->provide_creds(creds);
        
        std::thread connect_thread([this]() {
            std::cout << get_log_prefix() << " connect_phase2 thread started" << std::endl;
            try {
                impl_->core_client->connect();
            } catch (const std::exception& e) {
                std::cerr << "Background connect_phase2 error: " << e.what() << std::endl;
            }
        });
        connect_thread.detach();
        
        impl_->core_client->wait_for_connected();
    } else {
#ifdef ENABLE_DBUS
        // D-Bus mode
        impl_->dbus_client = std::make_shared<DBusClient>();
        impl_->dbus_client->set_log_level(g_log_level);
        impl_->dbus_client->set_log_callback([this](const std::string& msg) {
            emit_log(msg);
        });
        impl_->dbus_client->connect(config_path_, state_id, token, remote_ip);
#else
        throw std::runtime_error("D-Bus support is not enabled in this build");
#endif
    }
}

void OpenVPNClient::disconnect() {
    if (impl_->core_client) {
        impl_->core_client->stop();
    }
#ifdef ENABLE_DBUS
    if (impl_->dbus_client) {
        impl_->dbus_client->disconnect();
    }
#endif
}

} // namespace openlawsvpn
