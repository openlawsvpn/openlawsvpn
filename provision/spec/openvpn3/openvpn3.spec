# Package the SELinux policy
%bcond_without selinux
%global selinuxtype targeted

%bcond_with aws
%bcond_with devposture
%bcond_with dco
%bcond_with unittests
%bcond_with client
%bcond_with devel

%global _hardened_build 1
%global _vpath_srcdir %{name}-linux-%{version}%{?versiontag}

Name:           openvpn3
Version:        27
Release:        2%{?releasetag}%{?dist}
Summary:        OpenVPN 3 - TLS based VPN

License:        AGPL-3.0-only
URL:            https://codeberg.org/OpenVPN/openvpn3-linux/
Source0:        https://swupdate.openvpn.net/community/releases/openvpn3-linux-%{version}%{?versiontag}.tar.xz
Source1:        https://swupdate.openvpn.net/community/releases/openvpn3-linux-%{version}%{?versiontag}.tar.xz.asc
Source2:        https://gitlab.com/dazo/copr-openvpn3/-/raw/master/gpgkey-F554A3687412CFFEBDEFE0A312F5F7B42F2B01E7.gpg
Patch0:         https://gitlab.com/dazo/copr-openvpn3/-/raw/master/fedora-crypto-policy-compliance.patch
# Source100: local patches bundled via git_pack (gcc-16-array-bounds.patch etc.)
Source100:      {{{ git_pack path="provision/spec/openvpn3" }}}
Vendor:         OpenVPN Inc


# Code is not buildable on 32-bit architectures
ExcludeArch:    armv7hl i686

BuildRequires:  gnupg2
BuildRequires:  meson
BuildRequires:  gcc-c++
BuildRequires:  dbus-devel
BuildRequires:  glib2-devel
BuildRequires:  gdbuspp-devel >= 3
BuildRequires:  jsoncpp-devel
BuildRequires:  libcap-ng-devel
BuildRequires:  libuuid-devel
BuildRequires:  lz4-devel
BuildRequires:  openssl-devel
BuildRequires:  systemd
BuildRequires:  systemd-devel
BuildRequires:  zlib-devel
BuildRequires:  python3-dbus
BuildRequires:  python3-devel
BuildRequires:  python3-docutils
BuildRequires:  python3-jinja2
BuildRequires:  libnl3-devel
BuildRequires:  protobuf-devel

BuildRequires:  tinyxml2-devel

# DCO support dependencies
%if %{with dco}
Recommends:     kmod-ovpn-dco >= 0.2
%endif
# End - DCO support deps


Requires:       dbus
Requires:       gdbuspp >= 3
Requires:       polkit
Requires:       python(abi) >= 3.6
Requires:       python3-gobject-base
Requires:       python3-systemd
Requires(pre):  shadow-utils
Requires(pre):  /usr/bin/getent
Requires(postun): shadow-utils
Requires(postun): /usr/bin/getent
Provides:       group(openvpn)
Provides:       user(openvpn)

%if %{with selinux}
Requires:       %{name}-selinux >= %{version}-%{release}
%endif


%description
Next generation OpenVPN client, targeting modern Linux distributions.
OpenVPN 3 aims to be protocol compatible with the older OpenVPN 2.x
releases, but may not support all features of OpenVPN 2.x.

%if %{with devel}
%package devel
Summary:       Development headers for OpenVPN 3 Linux
BuildArch:     noarch

%description devel
Contains generated C header file needed to have correct mapping of
constants used by OpenVPN 3 Linux.
%endif


%if %{with client}
%package client
Summary:       OpenVPN 3 Client, TLS based VPN client
Requires:      %{name}%{?_isa} = %{version}-%{release}

%description client
OpenVPN 3 Client components.  Provides the binaries and D-Bus services
required to initiate and manage VPN client configuration profiles.
%endif

# openvpn3-addon-aws sub-package
%if %{with aws}
%package addon-aws
Summary:       OpenVPN 3 Linux AWS VPC integration support

%description addon-aws
This OpenVPN 3 Linux add-on will push VPN routes to the
AWS VPC to enable hosts inside the related VPC to utilize
the VPN setup.
%endif


%if %{with devposture}
%package addon-devposture
Summary:       OpenVPN 3 Linux Device Posture support

%description addon-devposture
This OpenVPN 3 Linux add-on enables clients to run certain
checks locally during the connection phase.  Which checks
is run is defined by a device posture protocol definition.

This feature is not enabled by default, but need to be
explicitly enabled in the configuration profile by setting
the appropriate Enterprise Profile.

%package dpc-openvpninc
Summary:       Device Posture profile for OpenVPN Inc service
BuildArch:     noarch

%description dpc-openvpninc
This contains the 'openvpninc' Device Posture Enterprise Profile
used by Cloud Connexa and OpenVPN Access Server
%endif


# SELinux sub-package
%if %{with selinux}
%package selinux
Summary:        OpenVPN 3 Linux SELinux policy
BuildArch:      noarch
BuildRequires:  selinux-policy-devel
Requires:       selinux-policy-%{selinuxtype}
%{?selinux_requires}

%description selinux
Additional SELinux policy required for OpenVPN 3 Linux to function
when SELinux is active.

# SELinux sub-package end
%endif

%prep
gpgv2 --quiet --keyring %{SOURCE2} %{SOURCE1} %{SOURCE0}
%autosetup -c -N
tar -xf %{SOURCE100}

pushd %{name}-linux-%{version}%{?versiontag}
%patch 0 -p1
%if 0%{?fedora} > 43
patch -p1 < ../openlawsvpn-provision-spec-openvpn3/gcc-16-array-bounds.patch
%endif
popd

%build
%meson -Dtest_programs=disabled -Dbash-completion=enabled \
       %{!?with_unittests:-Dunit_tests=disabled} \
       %{?with_selinux:-Dselinux_policy=enabled} \
       %{?with_dco:-Ddco=enabled} \
       %{?with_aws:-Daddon-aws=enabled} \
       %{?with_devposture:-Daddon-deviceposture=enabled}
%meson_build


%check
%meson_test --no-suite dbus --no-suite post-install

printf '\n\n\n\ ----------   Unit tests   ----------\n'
# Exclude PlatformInfo.DBus; it requires D-Bus available
pushd %{_vpath_builddir}
%if %{with unittests}
src/tests/unit/unit-tests --gtest_filter=-PlatformInfo.DBus
%endif
src/tests/logevent-selftest

printf '\n\n\n --------   Version checks   --------\n'
src/client/openvpn3-service-backendstart --version
src/client/openvpn3-service-client --version
src/netcfg/openvpn3-service-netcfg --version
%{python3} -c "from src.python.openvpn3.constants import VERSION; print('Python constants version: {ver}'.format(ver=VERSION))"
src/ovpn3cli/openvpn3 version
src/ovpn3cli/openvpn3-admin version
printf '\n\n'
popd


%install
rm -rf $RPM_BUILD_ROOT
%meson_install
mkdir -p %{buildroot}%{_sysconfdir}/%{name}/autoload
mkdir -p %{buildroot}%{_sharedstatedir}/%{name}

# Prepare some docs for the -devel sub-package
mkdir -p %{buildroot}/%{_pkgdocdir}-devel
mv -v %{buildroot}/%{_pkgdocdir}/dbus %{buildroot}/%{_pkgdocdir}-devel/

%if %{with devposture}
    mkdir -p %{buildroot}/%{_pkgdocdir}-addon-devposture \
          %{buildroot}/%{_pkgdocdir}-dpc-openvpninc
    mv -v %{buildroot}/%{_pkgdocdir}/device-posture/profile-format.md %{buildroot}/%{_pkgdocdir}-addon-devposture
    cp -v %{buildroot}/%{_pkgdocdir}/COPYRIGHT.md %{buildroot}/%{_pkgdocdir}-dpc-openvpninc
%endif


%pre
# Ensure we have openvpn user and group accounts without pulling in openvpn v2
# We use shadow-utils directly to avoid dependency on openvpn package
getent group openvpn &>/dev/null || groupadd -r openvpn
getent passwd openvpn &>/dev/null || \
    /usr/sbin/useradd -r -g openvpn -s /sbin/nologin -c OpenVPN \
        -d %{_sharedstatedir}/%{name} openvpn
exit 0

%post
LOGDEST="%{_sharedstatedir}/%{name}/openvpn3-init-config.log"
echo "" >> "$LOGDEST"
echo "** openvpn3-admin init-config start -- `date`" >> "$LOGDEST"
%{_sbindir}/openvpn3-admin version >> "$LOGDEST" 2>&1
%{_sbindir}/openvpn3-admin init-config --write-configs >> "$LOGDEST" 2>&1
echo "** openvpn3-admin init-config done (exit-code: $?)" >> "$LOGDEST"
exit 0


%preun
#
#  SELinux sub-package
#
%if %{with selinux}
%post selinux
# Install SELinux policy
%selinux_modules_install -s %{selinuxtype} %{_datadir}/selinux/packages/%{name}.pp.bz2
%selinux_modules_install -s %{selinuxtype} %{_datadir}/selinux/packages/%{name}_service.pp.bz2

# Enable the dbus_access_tuntap_device SELinux boolean.
# This is needed to make it possbile for the netcfg service
# to pass the file descriptor to tun devices it has created
# and configured.
%selinux_set_booleans -s %{selinuxtype} dbus_access_tuntap_device=1

%postun selinux
# Unset dbus_access_tuntap_device SELinux boolean and uninstall the policy
%selinux_unset_booleans -s %{selinuxtype} dbus_access_tuntap_device=1
%selinux_modules_uninstall -s %{selinuxtype} %{_datadir}/selinux/packages/%{name}.pp.bz2
%selinux_modules_uninstall -s %{selinuxtype} %{_datadir}/selinux/packages/%{name}_service.pp.bz2

%endif


%files
%config(noreplace) %{_datarootdir}/dbus-1/system.d/net.openvpn.v3.conf
%config(noreplace) %{_datarootdir}/dbus-1/system.d/net.openvpn.v3.configuration.conf
%config(noreplace) %{_datarootdir}/dbus-1/system.d/net.openvpn.v3.log.conf
%config(noreplace) %{_datarootdir}/dbus-1/system.d/net.openvpn.v3.netcfg.conf
%config %dir %{_sysconfdir}/%{name}
%config %dir %{_sysconfdir}/%{name}/autoload
%{_libexecdir}/openvpn3-linux/openvpn3-service-log
%{_libexecdir}/openvpn3-linux/openvpn3-service-configmgr
%{_libexecdir}/openvpn3-linux/openvpn3-service-netcfg
%{_libexecdir}/openvpn3-linux/openvpn3-service-backendstart
%{_libexecdir}/openvpn3-linux/openvpn3-service-client
%{_libexecdir}/openvpn3-linux/openvpn3-service-sessionmgr
%{_libexecdir}/openvpn3-linux/openvpn3-systemd
%{_unitdir}/openvpn3-autoload.service
%{_unitdir}/openvpn3-session@.service
%{_bindir}/openvpn3
%{_sbindir}/openvpn3-admin
%{_sbindir}/openvpn3-autoload
%{python3_sitelib}/openvpn3/*.py
%{python3_sitelib}/openvpn3/__pycache__/*
%{_datarootdir}/bash-completion/completions/openvpn*
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.configuration.service
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.log.service
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.netcfg.service
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.backends.service
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.sessions.service
%{_datarootdir}/dbus-1/system.d/net.openvpn.v3.backends.conf
%{_datarootdir}/dbus-1/system.d/net.openvpn.v3.client.conf
%{_datarootdir}/dbus-1/system.d/net.openvpn.v3.sessions.conf
%{_datarootdir}/polkit-1/rules.d/net.openvpn.v3.rules
%dir %attr(750, openvpn, openvpn)%{_sharedstatedir}/%{name}
%dir %attr(750, openvpn, openvpn)%{_sharedstatedir}/%{name}/configs
%ghost %config(noreplace) %{_sharedstatedir}/%{name}/log-service.json
%ghost %config(noreplace) %{_sharedstatedir}/%{name}/netcfg.json

%{_pkgdocdir}/COPYRIGHT.md
%{_pkgdocdir}/README.md
%{_pkgdocdir}/QUICK-START.md
%{_mandir}/man7/openvpn3-linux.7*
%{_mandir}/man1/openvpn3.1*
%{_mandir}/man1/openvpn3-config*.1*
%{_mandir}/man1/openvpn3-log.1*
%{_mandir}/man1/openvpn3-session*.1*
%{_mandir}/man8/openvpn3-admin*.8*
%{_mandir}/man8/openvpn3-autoload.8*
%{_mandir}/man8/openvpn3-service-log.8*
%{_mandir}/man8/openvpn3-service-configmgr.8*
%{_mandir}/man8/openvpn3-service-netcfg.8*
%{_mandir}/man8/openvpn3-service-backendstart.8*
%{_mandir}/man8/openvpn3-service-client.8*
%{_mandir}/man8/openvpn3-service-sessionmgr.8*
%{_mandir}/man8/openvpn3-systemd.8*

%exclude %{_bindir}/openvpn2
%exclude %{_bindir}/openvpn3-as
%exclude %{_bindir}/openvpn3-desktop-session-watcher
%exclude %{_userunitdir}/openvpn3-desktop-session-watcher.service
%exclude %{_mandir}/man1/openvpn2.1*
%exclude %{_mandir}/man1/openvpn3-as.1*
%exclude %{_mandir}/man1/openvpn3-desktop-session-watcher.1*
%exclude %{_includedir}/openvpn3/constants.h
%exclude %{_pkgdocdir}-devel/dbus/dbus-logging.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-overview.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-primer.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-service-net.openvpn.v3.backends.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-service-net.openvpn.v3.client.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-service-net.openvpn.v3.configuration.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-service-net.openvpn.v3.log.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-service-net.openvpn.v3.netcfg.md
%exclude %{_pkgdocdir}-devel/dbus/dbus-service-net.openvpn.v3.sessions.md
%exclude %{_mandir}/man8/openvpn3-service-devposture.8.gz


%if %{with devel}
%files devel
%{_pkgdocdir}-devel/
%{_includedir}/openvpn3
%endif

%if %{with client}
%files client
%{_bindir}/openvpn2
%{_bindir}/openvpn3-as
%{_bindir}/openvpn3-desktop-session-watcher
%{_sbindir}/openvpn3-autoload
%{_libexecdir}/openvpn3-linux/openvpn3-service-backendstart
%{_libexecdir}/openvpn3-linux/openvpn3-service-client
%{_libexecdir}/openvpn3-linux/openvpn3-service-sessionmgr
%{_libexecdir}/openvpn3-linux/openvpn3-systemd
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.backends.service
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.sessions.service
%{_unitdir}/openvpn3-session@.service
%{_userunitdir}/openvpn3-desktop-session-watcher.service
%{_mandir}/man1/openvpn2.1*
%{_mandir}/man1/openvpn3-as.1*
%{_mandir}/man1/openvpn3-desktop-session-watcher.1*
%{_mandir}/man8/openvpn3-service-backendstart.8*
%{_mandir}/man8/openvpn3-service-client.8*
%{_mandir}/man8/openvpn3-service-sessionmgr.8*
%{_mandir}/man8/openvpn3-systemd.8*
%endif

%if %{with aws}
%files addon-aws
%config(noreplace) %{_datarootdir}/dbus-1/system.d/net.openvpn.v3.aws.conf
%{_libexecdir}/openvpn3-linux/openvpn3-service-aws
%{_unitdir}/openvpn3-aws.service
%{_mandir}/man8/openvpn3-service-aws.8*
%{_sysconfdir}/%{name}/awscerts
%endif

%if %{with devposture}
%files addon-devposture
%config(noreplace) %{_datarootdir}/dbus-1/system.d/net.openvpn.v3.devposture.conf
%{_libexecdir}/openvpn3-linux/openvpn3-service-devposture
%{_datarootdir}/dbus-1/system-services/net.openvpn.v3.devposture.service
%{_mandir}/man8/openvpn3-service-devposture.8*
%dir %attr(750, openvpn, openvpn)%{_sharedstatedir}/%{name}/deviceposture
%dir %attr(750, openvpn, openvpn)%{_sharedstatedir}/%{name}/deviceposture/profiles
%{_sharedstatedir}/%{name}/deviceposture/profiles/example*.json
%{_pkgdocdir}-addon-devposture/profile-format.md

%files dpc-openvpninc
%{_pkgdocdir}-dpc-openvpninc/COPYRIGHT.md
%{_sharedstatedir}/%{name}/deviceposture/profiles/openvpninc.json
%endif

%if %{with selinux}
%files selinux
%{_datadir}/selinux/packages/%{name}.pp.*
%{_datadir}/selinux/packages/%{name}_service.pp.*
%ghost %{_sharedstatedir}/selinux/%{selinuxtype}/active/modules/200/%{name}
%ghost %{_sharedstatedir}/selinux/%{selinuxtype}/active/modules/200/%{name}_service
%endif


%changelog
* Thu Apr 16 2026 openlawsvpn contributors - 27-2
- Fix GCC-16 false-positive -Werror=array-bounds in openvpn3-core unicode-impl.hpp

* Tue Mar 3 2026  David Sommerseth <dazo@eurephia.org> - 27-1
- Release of OpenVPN 3 Linux v27
- Adds a new openvpn3-desktop-session-watcher notification tool

* Tue Sep 23 2025  David Sommerseth <dazo@eurephia.org> - 26-1
- Release of OpenVPN 3 Linux v26
- Removing the EPEL-10 SELinux policy packaging hack; new Copr build chroots are available
- Requring GDBus++ v3 or newer; this change was forgotten in openvpn3-25-1.

* Mon Jul 14 2025  David Sommerseth <davids@openvpn.net> - 25-1
- Release of OpenVPN 3 Linux v25
- Added EPEL-10 SELinux policy packaging hack
- Disabling unit tests on RHEL-8, due to googletest framework compat issues

* Mon May 19 2025  David Sommerseth <davids@openvpn.net> - 24.1-1
- Security: Fixes openvpn3-admin init-config symlink following {CVE-2025-3908}
- Bugfix: Fix openvpn3-service-sessionmgr crashing setting invalid log level
  for a running session
- Bugfix: Filter out ASCII control characters from user provided input
- Bugfix: Fix openvpn3-service-backendstart crash during shutdown
- Bugfix: Fix VPN sessions failing to start on systems without systemd-hostnamed
- Various build fixes, enabling GCC-15 builds (removing packaging patch)

* Mon Mar 24 2025  David Sommerseth <davids@openvpn.net> - 24-2
- Update build for Fedora 42+ with GCC-15
- logmetadata.hpp needed to include cstdint
- Disabling unit-tests for Fedora 42+ due to cstdint missing in googletest framework

* Tue Dec  3 2024  David Sommerseth <davids@openvpn.net> - 24-1
- Release of OpenVPN 3 Linux v24

* Wed Aug 28 2024 David Sommerseth <davids@openvpn.net> - 23-1
- Release of OpenVPN 3 Linux v23

* Mon Jun 17 2024 David Sommerseth <davids@openvpn.net> - 22-1.dev1
- Release of OpenVPN 3 Linux v22_dev

* Mon Oct 16 2023 David Sommerseth <davids@openvpn.net> - 21-3
- Don't depend on tinyxml2-7.0.0 on RHEL-8; use tinyxml2-6 from CodeReady repos

* Mon Oct 9 2023 David Sommerseth <davids@openvpn.net> - 21-2
- Build using devtoolset-11

* Fri Sep 22 2023 David Sommerseth <davids@openvpn.net> - 21-1
- Package the openvpn3-linux-21 stable release

* Fri Apr 21 2023 David Sommerseth <davids@openvpn.net> - 20-2
- Packaging fix only - Recommend kmod-ovpn-dco < 0.2
  - openvpn3 v20 is not ready for newer kmod-ovpn-dco versions yet
  - Improved descriptions

* Mon Mar 20 2023 David Sommerseth <davids@openvpn.net> - 20-1
- Package the openvpn3-linux-20 stable release

* Fri Oct 28 2022 David Sommerseth <davids@openvpn.net> - 19-1.beta
- Package the openvpn3-linux-19_beta release

* Tue Jun 7 2022 David Sommerseth <davids@openvpn.net> - 18-1.beta
- Package the openvpn3-linux-18_beta release
- D-Bus policies has been relocated to %%{_datarootdir}/dbus-1/system.d

* Mon Dec 13 2021 David Sommerseth <davids@openvpn.net> - 17-2.beta1
- Package the openvpn3-linux-17_beta release
- Build EPEL-7 using devtoolset-10

* Tue Oct 19 2021 David Sommerseth <davids@openvpn.net> - 16-1.beta1
- Package openvpn3-linux-16_beta release
- Include packaging of the new openvpn3-systemd integration

* Wed Jul 14 2021 David Sommerseth <davids@openvpn.net> - 15-1.beta1
- Package openvpn3-linux-15_beta release

* Wed Jul 14 2021 David Sommerseth <davids@openvpn.net> - 15-0.beta1
- Package openvpn3-linux-15_beta release

* Wed Jul 7 2021 David Sommerseth <davids@openvpn.net> - 14-0.beta1
- Package openvpn3-linux-14_beta release

* Sat Dec  5 2020 David Sommerseth <davids@openvpn.net> - 13-0.beta1
- Package openvpn3-linux-13_beta release

* Mon Nov 16 2020 David Sommerseth <davids@openvpn.net> - 12-0.beta1
- Package openvpn3-linux-12_beta release

* Fri Oct 30 2020 David Sommerseth <davids@openvpn.net> - 11-0.beta1
- Package openvpn3-linux-11_beta release
- Enable building with DCO support on Fedora and EL-8
- Ensure D-Bus policies are packaged in the proper sub-package, not all in the main package.
- Ensure openvpn3 man pages are in the proper sub-package

* Sat Jul 25 2020 David Sommerseth <davids@openvpn.net> - 10-0.beta1
- Package openvpn3-linux-10_beta release
- Move openvpn3 binary from -client to the base package
- Install bash-completions for openvpn2 in addition
- Install additional AWS region certificates in sysconfdir (openvpn3-addon-aws)
- Build RHEL-7 packages with -std=c++1y

* Tue Apr 28 2020 David Sommerseth <davids@openvpn.net> - 9-6.beta1
- Add explicit dependency on python3-gobject-base

* Tue Apr 28 2020 David Sommerseth <davids@openvpn.net> - 9-5.beta1
- Add explicit dependency on python3-dbus

* Tue Apr 28 2020 David Sommerseth <davids@openvpn.net> - 9-4.beta1
- Make use of the %%{selinux_requires} macro for SELinux dependency handling in the -selinux sub-package

* Tue Apr 28 2020 David Sommerseth <davids@openvpn.net> - 9-3.beta1
- Fix various openvpn3-selinux dependency related issues

* Sat Apr 25 2020 David Sommerseth <davids@openvpn.net> - 9-2.beta1
- Remove the Fedora packaging OpenSSL compliance patch on all distro releases older than Fedora 32

* Thu Apr 23 2020 David Sommerseth <davids@openvpn.net> - 9-1.beta1
- Packaging of the openvpn3-linux-9_beta release
- Reworked sub-packaging slightly, use proper bcond macros
- Added the new openvpn3-addon-aws sub-package

* Thu Feb 20 2020 David Sommerseth <davids@openvpn.net> - 8-2.beta1
- Package SELinux policy in a separate package

* Thu Feb 20 2020 David Sommerseth <davids@openvpn.net> - 8-1.beta1
- Adhere to Fedora Crypto Policy, using PROFILE=SYSTEM for cipher list init

* Mon Feb 10 2020 David Sommerseth <davids@openvpn.net> - 8-0.beta1
- Packaging of the openvpn3-linux-8_beta release
- Added additional compiler flags specific for RHEL-7

* Wed Dec 11 2019 David Sommerseth <davids@openvpn.net> - 7-0.beta1
- Packaging of the openvpn3-linux-7_beta release
- Corrected incorrect packaging of openvpn3-autoload.service file

* Fri May 24 2019 David Sommerseth <davids@openvpn.net> - 6-0.beta1
- Packaging of the openvpn3-linux-6_beta release
- This moves over to OpenSSL 1.1 on distributions providing that

* Wed Apr 3 2019 David Sommerseth <davids@openvpn.net> - 5-0.beta1
- Packaging of openvpn3-linux-5_beta release
- This release swaps out mbed TLS with OpenSSL
- Moving up to Python 3.6 dependency for RHEL 7

* Wed Mar 6 2019 David Sommerseth <davids@openvpn.net> - 4-0.beta2
- Added missing packaging of /var/lib/openvpn3/configs dir

* Fri Mar 1 2019 David Sommerseth <davids@openvpn.net> - 4-0.beta1
- Packaging of openvpn3-linux-4_beta release

* Thu Jan 31 2019 David Sommerseth <davids@openvpn.net> - 3-0.beta1
- Packaging of openvpn3-linux-3_beta release

* Wed Jan 30 2019 David Sommerseth <davids@openvpn.net> - 2-0.beta1
- Packaging of openvpn3-linux-2_beta release

* Sat Dec 8 2018 David Sommerseth <davids@openvpn.net> - 1-0.beta1
- First openvpn3-linux-1_beta release
