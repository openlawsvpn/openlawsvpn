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
