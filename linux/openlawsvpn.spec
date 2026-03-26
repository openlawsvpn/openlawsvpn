# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2024 openlawsvpn contributors
# See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
Name:           openlawsvpn
Version:        1.0.0
Release:        4%{?dist}
Summary:        Custom OpenVPN 3 Client with SAML support

# GUI is disabled by default
%bcond_with gui

License:        LGPL-2.1-or-later
URL:            https://github.com/openlaws/openlawsvpn
Source0:        {{{ git_repo_pack }}}
Source1:        {{{ git_pack path="openvpn3-core" }}}

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  openssl-devel
BuildRequires:  lz4-devel
BuildRequires:  asio-devel
BuildRequires:  glib2-devel
BuildRequires:  chrpath

%if %{with gui}
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  libcanberra-devel
# and flutter...
%endif

%description
A specialized OpenVPN 3 client for Linux that handles SAML authentication
and establishes tunnels via D-Bus or standalone mode.

# openlawsvpn (the core and CLI) should not have GUI dependencies.
# The GUI package is optional and separated.

%package cli
Summary:        Command line interface for openlawsvpn
# Ensure it only requires what's needed for the CLI
# NotRequires:       openvpn3-linux
# NotRequires:       xdg-utils

%description cli
Command line interface and core library for openlawsvpn.

%if %{with gui}
%package gui
Summary:        Graphical user interface for openlawsvpn
Requires:       %{name}-cli = %{version}-%{release}

%description gui
Graphical user interface for openlawsvpn based on Flutter.
%endif

%prep
{{{ git_repo_setup_macro }}}
mkdir -p openvpn3-core
tar -xf %{SOURCE1} -C openvpn3-core --strip-components=1

%build
cd linux
%cmake -DOPENVPN_DEFAULT_MODE=DBUS -Wno-dev
%cmake_build
cd ..

%if %{with gui}
# Build GUI
# Note: This assumes flutter is available in the environment
cd gui
# Clear flags that confuse clang (used by flutter/cmake for some parts)
export CFLAGS=""
export CXXFLAGS=""
export LDFLAGS=""
%{?flutter_bin}%{!?flutter_bin:flutter} pub get
%{?flutter_bin}%{!?flutter_bin:flutter} build linux --release --no-pub
cd ..
%endif

%install
cd linux
%cmake_install
cd ..

%if %{with gui}
# Install GUI
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_libdir}/openlawsvpn
cp gui/build/linux/x64/release/bundle/openlawsvpn-gui %{buildroot}%{_libdir}/openlawsvpn/
cp -r gui/build/linux/x64/release/bundle/data %{buildroot}%{_libdir}/openlawsvpn/
cp -r gui/build/linux/x64/release/bundle/lib/* %{buildroot}%{_libdir}/openlawsvpn/

# Fix RPATH for the GUI binary and its plugins
find %{buildroot}%{_libdir}/openlawsvpn/ -name "*.so" -exec chrpath --delete {} \;
chrpath --replace '$ORIGIN/../openlawsvpn' %{buildroot}%{_libdir}/openlawsvpn/openlawsvpn-gui || \
chrpath --delete %{buildroot}%{_libdir}/openlawsvpn/openlawsvpn-gui

# Create a symlink
ln -s %{_libdir}/openlawsvpn/openlawsvpn-gui %{buildroot}%{_bindir}/openlawsvpn-gui
%endif

%files cli
%{_bindir}/openlawsvpn-cli
%{_libdir}/libopenlawsvpn.so

%if %{with gui}
%files gui
%{_bindir}/openlawsvpn-gui
%{_libdir}/openlawsvpn/
%endif

%changelog
* Tue Mar 24 2026 Anatolii Vorona <vorona.tolik@gmail.com> - 1.0.0-4
- Remove unused dependency: fmt-devel, openvpn3-client
* Mon Mar 23 2026 Anatolii Vorona <vorona.tolik@gmail.com> - 1.0.0-2
- Add openlawsvpn-gui to the package
* Sun Jun 22 2025 Anatolii Vorona <vorona.tolik@gmail.com> - 1.0.0-1
- Initial RPM release with D-Bus and SAML support
