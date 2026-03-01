# openlawsvpn — AWS Client VPN for Linux

A specialized OpenVPN 3 based client for AWS Client VPN endpoints with native SAML authentication support. It provides both a modern Flutter GUI and a command-line interface.

## Key Features

- **Native SAML Support**: Automatically handles the "AWS Dance" (CRV1 protocol) by launching a browser and capturing tokens on port 35001.
- **Modern GUI**: Built with Flutter and Material 3 for a smooth Linux desktop experience.
- **Unprivileged Operation**: Integrates with `openvpn3-linux` D-Bus services to run without root/sudo.
- **Log Management**: Real-time log console with automatic filtering of binary/SSL garbage.
- **Dual Modes**: Supports both D-Bus (recommended) and Direct (standalone) connection modes.

## Installation & Build

The project uses a unified Makefile to build both the C++ core and the Flutter GUI.

### Requirements
- **Build Tools**: `cmake`, `ninja-build`, `gcc-c++`, `make`, `curl`
- **Libraries**: `openssl-devel`, `lz4-devel`, `glib2-devel`, `gtk3-devel`, `libcanberra-devel`
- **SDK**: `flutter` (for the GUI)
- **Runtime**: `openvpn3-linux` (for D-Bus mode)

### Quick Build
```bash
# Build both CLI and GUI
make all
```

The build artifacts will be available in:
- CLI: `build/bin/openlawsvpn-cli`
- GUI: `build/gui/openlawsvpn-gui`

## Usage

### GUI (Recommended)
Launch the GUI application:
```bash
./build/gui/openlawsvpn-gui
```
1. Select your `.ovpn` profile.
2. Click **Connect**.
3. A browser will open for SAML authentication.
4. Once authenticated, the VPN will connect automatically.

### CLI
Connect using the command-line tool:
```bash
./build/bin/openlawsvpn-cli connect /path/to/your/profile.ovpn
```

## Project Structure
- `gui/`: Flutter-based graphical interface.
- `linux/`: C++ core implementation and D-Bus client.
- `docs/`: Technical specifications and packaging files.
- `PLAN.md`: Detailed implementation status and architecture overview.

## License

openlawsvpn is licensed under the **GNU Lesser General Public License v2.1
or later** ([LGPL-2.1-or-later](LICENSE)).

You may use this software freely in any environment — including cloud VMs,
containers, CI/CD pipelines, and automation scripts — without any
source-disclosure obligations on your own code. See
[LICENSE_USAGE_EXCEPTION](LICENSE_USAGE_EXCEPTION) for the explicit grant.

Modifications to openlawsvpn itself must be released under LGPL-2.1-or-later.

Commercial licenses (no copyleft obligations, enterprise support SLA) are
available for organizations that need them. Contact the maintainers.
