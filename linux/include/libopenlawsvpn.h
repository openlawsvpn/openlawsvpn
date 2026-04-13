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

#pragma once
#include <string>
#include <functional>
#include <vector>

namespace openlawsvpn {

enum class ConnectMode {
    DIRECT,
    DBUS
};

typedef void (*LogCallback)(const char* message, void* user_data);

/**
 * Platform-independent description of a VPN tunnel interface.
 * Populated from openvpn3-core's TunBuilderCapture before invoking
 * TunEstablishFn. Callers need no openvpn3-core headers.
 */
struct TunConfig {
    struct IpNet {
        std::string address;
        int         prefix_length = 0;
        bool        ipv6          = false;
    };
    int                      mtu             = 1500;
    std::string              session_name;
    std::vector<IpNet>       tunnel_addresses;   // local VPN IP address(es)
    std::vector<IpNet>       routes;             // routes to push into tunnel
    std::vector<std::string> dns_servers;
    std::vector<std::string> search_domains;
    bool                     reroute_gw_ipv4 = false;
    bool                     reroute_gw_ipv6 = false;
};

/**
 * Called when openvpn3-core requests tun interface creation.
 * Must return an open tun fd (caller takes ownership), or -1 on failure.
 *
 * Linux:   not needed — TunLinuxSetup is used when this is not set.
 * Android: VpnService.Builder.establish() → pfd.detachFd().
 * macOS:   NEPacketTunnelProvider / utun via openlawsvpnagent.
 */
using TunEstablishFn  = std::function<int(const TunConfig&)>;

/**
 * Called for each UDP/TCP socket openvpn3-core creates before connecting.
 * Must return true to allow the connection.
 *
 * Android: must call VpnService.protect(fd) to prevent routing loops.
 * Linux/macOS: returning true is sufficient.
 */
using SocketProtectFn = std::function<bool(int fd, const std::string& remote, bool ipv6)>;

class OpenVPNClient {
public:
    OpenVPNClient(const std::string& config_path);
    ~OpenVPNClient();

    void set_connect_mode(ConnectMode mode);
    void set_log_callback(LogCallback callback, void* user_data);
    void set_tun_establish_fn(TunEstablishFn fn);
    void set_socket_protect_fn(SocketProtectFn fn);

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

    // Blocks until the Phase 2 tunnel is torn down. Returns true if SAML
    // re-authentication is needed (e.g. session expired), false on permanent failure.
    bool wait_for_disconnect();

    // Read and filter config file (removes AWS-proprietary directives)
    static std::string read_and_filter_config(const std::string& path);

private:
    std::string config_path_;
    ConnectMode mode_;
    LogCallback   log_callback_      = nullptr;
    void*         user_data_         = nullptr;
    TunEstablishFn  tun_establish_fn_;
    SocketProtectFn socket_protect_fn_;
    struct Impl;
    Impl* impl_;

    void emit_log(const std::string& msg);
public:
    void emit_log_public(const std::string& msg) { emit_log(msg); }
};

std::string get_log_prefix();

} // namespace openlawsvpn
