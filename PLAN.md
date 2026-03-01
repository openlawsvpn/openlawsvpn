# openlawsvpn — Project Status

## Implementation Summary
- **GUI Application**: Developed using Flutter (Material 3) for a modern Linux desktop experience.
- **VPN Core Integration**: Integrated with the OpenVPN 3 C++ core via Dart FFI.
- **AWS SAML Flow**: Fully automated "AWS Dance" with an embedded HTTP server on port 35001.
- **D-Bus Support**: Reliable integration with `openvpn3-linux` services for unprivileged operation.
- **Log Management**: Robust log streaming with binary-data filtering and UTF-8 sanitization.

---

## 1. Project Background & Context
The `openlawsvpn` project is a specialized VPN client for AWS Client VPN endpoints, built on OpenVPN 3 Core.

**License:** LGPL-2.1-or-later with usage exception. Free to use in any
environment including cloud VMs, CI/CD, containers, and automation with
no source-disclosure obligations. Modifications to the client must be
released. See `LICENSE_USAGE_EXCEPTION`.

### Core Architecture
- **Language:** C++20 (Library), C (FFI Wrapper), Dart (GUI).
- **Library (`libopenlawsvpn`):** Handles VPN protocol, SAML challenge parsing, and D-Bus/Direct tunneling.
- **SAML Flow ("The AWS Dance"):**
  1. **Phase 1:** Triggered by `connect_phase1()`. Server returns a `DYNAMIC_CHALLENGE` with a SAML URL.
  2. **Token Capture:** A local HTTP server (port 35001) captures the SAML token from the browser redirect.
  3. **Phase 2:** Triggered by `connect_phase2()`. The captured token is used as the password.
  4. **Sticky IP:** The IP resolved in Phase 1 **must** be used in Phase 2 to prevent authentication failure.

### Connection Modes
- **DBUS (Default/Recommended for GUI):** establishment via `openvpn3-linux` system services. Unprivileged.
- **DIRECT:** Raw `openvpn3-core` establishment. Requires `root` or `CAP_NET_ADMIN`.

## 2. Implementation Details

### FFI API Surface (`libopenlawsvpn_ffi.cpp`):
- `openvpn_client_new(config_path)`
- `openvpn_client_free(handle)`
- `openvpn_client_set_connect_mode(handle, mode)`
- `openvpn_client_set_log_callback(handle, callback, user_data)`
- `openvpn_client_connect_phase1(handle)` -> Returns `saml_url`, `state_id`, `remote_ip`.
- `openvpn_client_connect_phase2(handle, state_id, token, remote_ip)`
- `openvpn_client_disconnect(handle)`

### GUI Features
- **Profile Selection:** File picker for `.ovpn` files.
- **Dashboard:** Status indicators (Disconnected, Connecting, Authenticating, Connected).
- **Log Console:** Scrollable terminal view with binary-log filtering (removes SSL/ASN.1 garbage).
- **SAML Integration:** Automatic browser launching and robust token capture (supports GET/POST).

## 3. Build & Packaging
- **Root Makefile:** Use `make gui` to build everything.
- **Artifacts:** All files are consolidated in the top-level `build/` directory.
- **Dependencies:** Requires `flutter`, `gtk3-devel`, `glib2-devel`, and `libcanberra-devel`.

## 4. Key Constraints & Notes
- **Permissions:** The GUI app runs as a regular user; it depends on `openvpn3-linux` D-Bus services.
- **Port 35001:** Exclusively used for SAML redirects as required by AWS.
- **Stability:** Includes FFI-safe callbacks and C++ exception handling to prevent crashes.
