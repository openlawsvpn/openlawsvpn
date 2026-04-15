# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2024 openlawsvpn contributors
# See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
Name:           openlawsvpn
Version:        1.0.0
Release:        6%{?dist}
Summary:        AWS Client VPN client with SAML/SSO support

License:        LGPL-2.1-or-later
URL:            https://github.com/openlawsvpn/openlawsvpn
Source0:        {{{ git_repo_pack }}}
Source1:        {{{ git_pack path="openvpn3-core" }}}

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  openssl-devel
BuildRequires:  lz4-devel
BuildRequires:  asio-devel
BuildRequires:  glib2-devel
BuildRequires:  chrpath

%description
openlawsvpn is a specialized OpenVPN 3 client for Linux that handles SAML
authentication (Okta, Azure AD, Google Workspace, any SAML 2.0 IdP) and
establishes tunnels via D-Bus (unprivileged) or standalone mode.

%package cli
Summary:        Command line interface for openlawsvpn

%description cli
Command line interface and shared library for openlawsvpn.

# GUI sub-package — GTK4 + libadwaita (Rust).
# Not yet built; placeholder for when gui-gtk/ is implemented (Phase 7).
# To enable: add BuildRequires for gtk4-devel, libadwaita-devel, cargo
# and uncomment the gui build/install sections below.
#
# package gui
# Summary:        Graphical user interface for openlawsvpn
# Requires:       openlawsvpn-cli
# Requires:       gtk4
# Requires:       libadwaita
#
# description gui
# Native GTK4 + libadwaita desktop GUI for openlawsvpn.
# Mirrors the Android app: profile list, connection states, live log, system tray.

%prep
{{{ git_repo_setup_macro }}}
mkdir -p openvpn3-core
tar -xf %{SOURCE1} -C openvpn3-core --strip-components=1

%build
cd linux
%cmake -DOPENVPN_DEFAULT_MODE=DBUS -Wno-dev
%cmake_build
cd ..

# GUI build (uncomment when gui-gtk/ exists):
# cargo build --release --manifest-path gui-gtk/Cargo.toml

%install
cd linux
%cmake_install
cd ..

# GUI install (uncomment when gui-gtk/ exists):
# install -Dm755 gui-gtk/target/release/openlawsvpn-gui %{buildroot}%{_bindir}/openlawsvpn-gui

%files cli
%{_bindir}/openlawsvpn-cli
%{_libdir}/libopenlawsvpn.so

# files gui
# /usr/bin/openlawsvpn-gui

%changelog
* Wed Apr 15 2026 openlawsvpn contributors - 1.0.0-6
- Remove Flutter GUI dependency; GUI now GTK4+libadwaita+Rust (Phase 7, not yet built)
* Fri Mar 27 2026 Anatolii Vorona <vorona.tolik@gmail.com> - 1.0.0-5
- Automatic SAML re-authentication on session expiry (NEED_CREDS/AUTH_FAILED after connect)
* Tue Mar 24 2026 Anatolii Vorona <vorona.tolik@gmail.com> - 1.0.0-4
- Remove unused dependency: fmt-devel, openvpn3-client
* Mon Mar 23 2026 Anatolii Vorona <vorona.tolik@gmail.com> - 1.0.0-2
- Add openlawsvpn-gui to the package
* Sun Jun 22 2025 Anatolii Vorona <vorona.tolik@gmail.com> - 1.0.0-1
- Initial RPM release with D-Bus and SAML support
