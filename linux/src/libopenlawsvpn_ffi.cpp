// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 openlawsvpn contributors
// See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
#include "libopenlawsvpn.h"
#include <cstring>
#include <cstdlib>

using namespace openlawsvpn;

extern "C" {

struct Phase1ResultC {
    char* saml_url;
    char* state_id;
    char* remote_ip;
};

void* openvpn_client_new(const char* config_path) {
    return new OpenVPNClient(config_path);
}

void openvpn_client_free(void* client) {
    delete static_cast<OpenVPNClient*>(client);
}

void openvpn_client_set_connect_mode(void* client, int mode) {
    static_cast<OpenVPNClient*>(client)->set_connect_mode(static_cast<ConnectMode>(mode));
}

void openvpn_client_set_log_level(void* client, int level) {
    static_cast<OpenVPNClient*>(client)->set_log_level(level);
}

void openvpn_client_set_log_callback(void* client, LogCallback callback, void* user_data) {
    static_cast<OpenVPNClient*>(client)->set_log_callback(callback, user_data);
}

Phase1ResultC openvpn_client_connect_phase1(void* client) {
    try {
        auto res = static_cast<OpenVPNClient*>(client)->connect_phase1();
        Phase1ResultC res_c;
        res_c.saml_url = strdup(res.saml_url.c_str());
        res_c.state_id = strdup(res.state_id.c_str());
        res_c.remote_ip = strdup(res.remote_ip.c_str());
        return res_c;
    } catch (...) {
        return {nullptr, nullptr, nullptr};
    }
}

void openvpn_client_connect_phase2(void* client, const char* state_id, const char* token, const char* remote_ip) {
    try {
        static_cast<OpenVPNClient*>(client)->connect_phase2(state_id, token, remote_ip ? remote_ip : "");
    } catch (const std::exception& e) {
        static_cast<OpenVPNClient*>(client)->emit_log_public(get_log_prefix() + " ConnectPhase2 ERROR: " + std::string(e.what()) + "\n");
    } catch (...) {
        static_cast<OpenVPNClient*>(client)->emit_log_public(get_log_prefix() + " ConnectPhase2 ERROR: Unknown exception\n");
    }
}

void openvpn_client_disconnect(void* client) {
    static_cast<OpenVPNClient*>(client)->disconnect();
}

void openvpn_free_string(char* s) {
    free(s);
}

}
