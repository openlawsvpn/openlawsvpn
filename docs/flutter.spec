%global debug_package %{nil}

Name:			flutter
Version:		3.41.5
Release:		1%?dist
Summary:		SDK for crafting beautiful, fast user experiences from a single codebase
License:		BSD-3-Clause
URL:			https://flutter.dev
Group:			Development/Building
Source0:		https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_%version-stable.tar.xz
Requires:		bash curl git file which zip xz
Recommends:		mesa-libGLU
AutoReqProv:	no

%description
Flutter transforms the app development process. Build, test, and deploy
beautiful mobile, web, desktop, and embedded apps from a single codebase.

%prep
%setup -q -c -n %name

%install
mkdir -p %buildroot/usr/lib/%name %buildroot%_bindir
cp -r %name/bin %name/packages %name/LICENSE  %name/CHANGELOG.md %buildroot/usr/lib/%name/
ln -s /usr/lib/%name/bin/%name %buildroot%_bindir/%name

%files
%_bindir/%name
/usr/lib/%name

%changelog
%autochangelog
