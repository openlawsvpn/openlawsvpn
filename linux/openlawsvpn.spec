# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2024 openlawsvpn contributors
# See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
Name:           openlawsvpn
Version:        1.0.0
Release:        9%{?dist}
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
BuildRequires:  cargo-rpm-macros
BuildRequires:  gtk4-devel
BuildRequires:  libadwaita-devel
BuildRequires:  clang-devel
BuildRequires:  rust-zbus+tokio-devel

%description
openlawsvpn is a specialized OpenVPN 3 client for Linux that handles SAML
authentication (Okta, Azure AD, Google Workspace, any SAML 2.0 IdP) and
establishes tunnels via D-Bus (unprivileged) or standalone mode.

%package cli
Summary:        Command line interface for openlawsvpn

%description cli
Command line interface and shared library for openlawsvpn.

%package gui
Summary:        Graphical user interface for openlawsvpn
Requires:       openlawsvpn-cli
Requires:       gtk4
Requires:       libadwaita
Requires:       gnome-shell-extension-appindicator

%description gui
Native GTK4 + libadwaita desktop GUI for openlawsvpn.
Profile management, SAML/SSO login, live connection log.
Requires the AppIndicator GNOME Shell extension for system tray support.

%prep
{{{ git_repo_setup_macro }}}
mkdir -p openvpn3-core
tar -xf %{SOURCE1} -C openvpn3-core --strip-components=1
cd gui-gtk && %cargo_prep && cd ..

%generate_buildrequires
cd gui-gtk && %cargo_generate_buildrequires && cd ..

%build
cd linux
%cmake -DOPENVPN_DEFAULT_MODE=DBUS -Wno-dev
%cmake_build
cd ..

LIB_DIR=$(realpath linux/redhat-linux-build)
sed -i "/^\[env\]/a OPENLAWSVPN_LIB_DIR = \"$LIB_DIR\"" gui-gtk/.cargo/config.toml
cd gui-gtk && %cargo_build && cd ..

%install
cd linux
%cmake_install
cd ..

cd gui-gtk && %cargo_install && cd ..

%files cli
%{_bindir}/openlawsvpn-cli
%{_libdir}/libopenlawsvpn.so

%files gui
%{_bindir}/openlawsvpn-gui

%changelog
* Thu Apr 16 2026 openlawsvpn contributors - 1.0.0-9
- Relax bindgen build-dep to >=0.69 to match whatever Fedora ships (fc44 does not have 0.71)
* Thu Apr 16 2026 openlawsvpn contributors - 1.0.0-8
- Switch GUI build to cargo-rpm-macros; upgrade to gtk4 0.10 / libadwaita 0.8
- Remove vendored Rust dependencies (use Fedora system registry)
* Wed Apr 15 2026 openlawsvpn contributors - 1.0.0-7
- Enable GTK4 + libadwaita GUI sub-package (openlawsvpn-gui)
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
