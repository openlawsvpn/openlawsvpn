#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
openvpn3 sessions-list | awk '/Path:/ {print $2}' | while read -r path; do
    openvpn3 session-manage -D --path "$path"
done

openvpn3 configs-list -v | awk '/^\/net\/openvpn/ {print $1}' | while read -r path; do
    echo YES | openvpn3 config-remove --path "$path"
done