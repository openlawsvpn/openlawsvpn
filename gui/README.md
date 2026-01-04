# openlawsvpn GUI (Linux)

This is a Flutter-based GUI for the `openlawsvpn` project, providing a graphical interface for connecting to AWS Client VPN endpoints using OpenVPN 3 Core.

## Features
- **Profile Selection**: Choose an `.ovpn` configuration file via a file picker.
- **Material 3 UI**: Modern dashboard with real-time connection status indicators.
- **Log Console**: scrollable terminal view with automatic binary-data filtering (hides SSL/ASN.1 garbage).
- **SAML Flow**: Fully automated "AWS Dance" with an embedded HTTP server (port 35001) and automatic browser launching.
- **Connection Modes**:
  - **D-Bus**: Recommended for unprivileged users (communicates with `openvpn3-linux` services).
  - **Direct**: Uses `openvpn3-core` directly (requires `root` or `CAP_NET_ADMIN`).

## Architecture
The GUI interacts with the C++ core through Dart FFI (Foreign Function Interface):
- `lib/bindings/libopenlawsvpn.dart`: Low-level mapping and log sanitization.
- `lib/services/vpn_service.dart`: High-level service managing the multi-phase connection state.
- `lib/services/saml_capture_service.dart`: Robust HTTP server (port 35001) for capturing SAML tokens via GET or POST.

## Build Requirements
- **Flutter SDK**: Required for compilation.
- **GTK3 Development Headers**: `sudo dnf install gtk3-devel` (Fedora/RHEL).
- **GLib/libcanberra Headers**: `sudo dnf install glib2-devel libcanberra-devel`.
- **C++ Library**: Must be built as a shared library (`libopenlawsvpn.so`).

## Building from the Root
It is recommended to build the GUI using the root `Makefile`:
```bash
make gui
```
The final bundle will be available in the top-level `build/gui/` directory.

## Running
After building, execute:
```bash
./build/gui/openlawsvpn-gui
```
The build process automatically bundles the necessary shared libraries in `build/gui/lib/`.

## Important Notes
- **SAML Port**: AWS Client VPN redirects strictly require port **35001**.
- **Permissions**: Ensure your user has permissions to access `openvpn3-linux` D-Bus interfaces.
- **Logs**: Binary log sequences (ASN.1/SSL) are automatically filtered to keep the console readable.
