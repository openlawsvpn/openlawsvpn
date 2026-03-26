# openlawsvpn — AWS Client VPN SAML Integration

This tool implements a two-phase OpenVPN-based client that integrates with AWS Client VPN using SAML authentication, as specified in the [technical specification](#technical-specification).

---

## Technical Specification: AWS Client VPN SAML Integration

### 1. Core Concept: The Two-Phase Connect Model
AWS Client VPN uses the CRV1 (Challenge-Response Version 1) protocol on top of OpenVPN. The connection occurs in two distinct phases:

#### Phase 1: The Challenge (Provocation)
*   **Goal:** Initiate an OpenVPN connection to the AWS endpoint to receive a SAML Identity Provider (IdP) URL.
*   **Authentication:** The client connects using `N/A` as the username and uses a special "trigger" string as the password: `ACS::35001`.
*   `ACS` indicates that the client supports the Assertion Consumer Service.
*   `::` is the separator used for the password format.
*   `35001` specifies the local port where the client's internal HTTP server will listen for the SAML response.
*   **Result:** The server rejects the connection but sends a `DYNAMIC_CHALLENGE` event containing a CRV1-formatted string.

#### Phase 2: The Response (Establishment)
*   **Goal:** Reconnect to the same AWS endpoint using the credentials obtained from the SAML login.
*   **Authentication:** 
    *   **Username:** Always use `N/A`.
    *   **Password:** A response string formatted as `CRV1::<state_id>::<token>`.
        *   `<state_id>` is an opaque session identifier received from the Phase 1 challenge.
        *   `<token>` is the base64-encoded SAML assertion (SAMLResponse) captured from the IdP.
*   **Result:** The server validates the SAML token and establishes the VPN tunnel.

---

### 2. Protocol Parsing: CRV1 Structure
The `DYNAMIC_CHALLENGE` string follows a colon-separated format:
`CRV1:<flags>:<state_id>:<base64_username>:<challenge_text>`

*   **Flags:** Usually `R` (Response required).
*   **State ID:** A unique session token that must be returned in Phase 2.
*   **Base64 Username:** The username provided in Phase 1, encoded in base64.
*   **Challenge Text:** For AWS SAML, this field contains the full URL of the Identity Provider (e.g., `https://portal.sso.region.amazonaws.com/...`).

---

### 3. Identity Provider (IdP) Interaction
To obtain the SAML token, the client facilitates a browser-based login:
1.  **Browser Launch:** Open the `challenge_text` URL in the system's default web browser.
2.  **User Login:** The user authenticates with their IdP (e.g., Okta, Azure AD, AWS SSO).
3.  **Redirection:** Upon success, the IdP redirects the browser to `http://127.0.0.1:35001`.
4.  **POST Data:** The redirect is typically an HTTP POST request containing a form-encoded body with a `SAMLResponse` field.

---

### 4. Internal HTTP Callback Server
The client runs a minimal, temporary HTTP server to capture the SAML token:
*   **Binding:** Listen on `127.0.0.1` at the port specified in Phase 1 (default: `35001`).
*   **Endpoint:** Accept any incoming POST request at the root path.
*   **Extraction:** 
    *   Parse the `application/x-www-form-urlencoded` body.
    *   Extract the value of the `SAMLResponse` key.
    *   URL-decode the value to get the raw base64 SAML assertion.
*   **Response:** Return a simple HTML "Success" page to the browser to inform the user they can close the tab.
*   **Lifecycle:** The server should start before Phase 1 ends and stop immediately after the token is captured or a timeout occurs.

---

### 5. Platform-Specific Tunnel Management (Linux)
On Linux, the client handles the network interface (TUN device) in two ways:

#### A. Direct Mode (Superuser)
*   Requires `CAP_NET_ADMIN` or root privileges.
*   The client creates a TUN device (e.g., `tun0`) directly via `openvpn3-core`.

#### B. D-Bus Mode (Unprivileged)
*   The default connection mode on Linux.
*   Does **not** require root or special capabilities, as it delegates tunnel establishment to the `openvpn3-linux` system services.
*   **Workflow:**
    1.  **Configuration Import:** The client sends the OVPN profile to the `net.openvpn.v3.configuration` service using the `Import` method.
    2.  **Session Creation:** It initiates a new tunnel session via the `net.openvpn.v3.sessions.NewTunnel` method.
    3.  **Synchronization:** The client waits for the backend process to initialize by polling the D-Bus object path for readiness.
    4.  **Credential Provision:** It injects the CRV1 response (the SAML token) directly into the backend via the `UserInputProvide` method.
    5.  **Signal Monitoring:** The client subscribes to D-Bus `StatusChange` and `Log` signals to monitor the connection progress in real-time.
    6.  **Connection Success:** Once the backend emits a "Connected" status or the `connected_to` property becomes valid, the client confirms the tunnel is up.
    7.  **Cleanup:** On disconnect or failure, the client calls the `Disconnect` method on the session object, ensuring the backend process and virtual interface are properly removed.
*   **Requirements:**
    *   The `openvpn3-linux` package must be installed and its services (`openvpn3-service-configmgr`, `openvpn3-service-sessionmgr`, `openvpn3-service-netcfg`) must be functional.
    *   The user must have appropriate D-Bus permissions to interact with the `net.openvpn.v3.*` namespaces (usually provided by default for logged-in users).

---

### 6. Operational State Machine
1.  **IDLE:** Client is ready.
2.  **PHASE_1_CONNECTING:** Start OpenVPN connection with username `N/A` and `ACS:35001` password.
3.  **WAITING_FOR_CHALLENGE:** Listen for the `DYNAMIC_CHALLENGE` event.
4.  **SAML_LOGIN_PENDING:** Start HTTP server, open browser with the SAML URL.
5.  **TOKEN_CAPTURED:** HTTP server receives `SAMLResponse`; stop server.
6.  **PHASE_2_CONNECTING:** Restart OpenVPN with username `N/A` and the formatted `CRV1:R:<state_id>:<token>` password.
7.  **CONNECTED:** Tunnel is established and routing is configured.
8.  **DISCONNECTING/CLEANUP:** Close the tunnel, release D-Bus sessions, and clean up temporary files.

---

### 7. Reliability and Edge Cases
*   **Port Conflicts:** If port `35001` is occupied, Phase 1 must fail gracefully.
*   **Stale Sessions:** On Linux D-Bus, ensure that failed or interrupted connection attempts clean up their session paths.
*   **Timeouts:** Implement a timeout (e.g., 120 seconds) for the user to complete the browser login.
*   **Headless Servers:** On systems without a GUI browser (`xdg-open` fails), the SAML URL must be printed to the terminal for manual copying.

---

## Build

Builds against openvpn3-core via a git submodule (`openvpn3-core/` at repo root),
pinned to the version matching the current openvpn3-linux release.

To update the openvpn3-core pin when a new openvpn3-linux is released:
```bash
make update-openvpn3-core OPENVPN3_LINUX_VERSION=v28
git commit -m "chore: bump openvpn3-core to match openvpn3-linux v28"
```

### Standard Build

From repository root:

```bash
git clone https://github.com/BOPOHA/openlawsvpn
make linux
```

By default, the client builds with D-Bus support (requires `glib2-devel` and `gio-2.0`).

Binary path: `build/bin/openlawsvpn-cli`

### Minimal Build (No D-Bus)

```bash
mkdir -p build/linux-nodbus
cmake -S linux -B build/linux-nodbus -DENABLE_DBUS=OFF -G Ninja
cmake --build build/linux-nodbus
```

In this mode, the client only supports Direct Mode (`--standalone`) and does not
require D-Bus or GLib at runtime.

### Build Requirements

*   `cmake`, `ninja-build`, `gcc-c++`, `git`
*   `openssl-devel`, `lz4-devel`
*   `glib2-devel` (and its dependencies, including `gio-2.0`)

### RPM Build

To create an RPM package for your distribution:

```bash
make linux-rpm
```

The resulting RPMs will be located in `rpmbuild/RPMS/`. This process requires `rpm-build` and all the build requirements listed above.

## Usage

```bash
./build/linux/openlawsvpn-cli connect docs/test.ovpn
```

### Options

*   `--log-level <level>` or `-l <level>`: Set log verbosity (0=None, 1=Info, 2=Debug).
*   `--standalone`: Force "Direct Mode" (requires root/CAP_NET_ADMIN) instead of the default D-Bus mode.

---
