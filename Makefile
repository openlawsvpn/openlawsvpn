.PHONY: all linux linux-static gui gui-check clean openvpn3-rpm linux-rpm linux-srpm linux-rpm-mock update-openvpn3-core

SPEC_FILE := linux/openlawsvpn.spec
PROJECTNAME := openlawsvpn
PROJECTTMPDIR := /tmp/$(PROJECTNAME)
RPM_VERSION := $(shell rpmspec --srpm -q --qf "%{Version}-%{Release}" $(SPEC_FILE))
FEDORA_VERSION := $(shell rpm -E %fedora)

BUILD_DIR ?= $(shell pwd)/build/linux
BUILD_DIR_STATIC ?= $(shell pwd)/build/linux-static
GUI_BUILD_DIR ?= $(shell pwd)/build/gui
CMAKE_INSTALL_PREFIX ?= $(shell pwd)/build

OPENVPN3_LINUX_VERSION ?= v27

OPENVPN3_RPM_SPEC := docs/openvpn3.spec
OPENVPN3_RPM_VERSION := $(shell rpmspec --srpm -q --qf "%{Version}-%{Release}" ${OPENVPN3_RPM_SPEC})

# Detect fast linker (mold is fastest, then lld)
LINKER := $(shell which mold 2>/dev/null || which lld 2>/dev/null)
# Detect compiler cache
CCACHE_BIN := $(shell which ccache 2>/dev/null)
# Use project-local ccache directory to avoid global cache influence
CCACHE_DIR := $(shell pwd)/.ccache
export CCACHE_DIR

all: linux

openvpn3-core/.git:
	git submodule update --init openvpn3-core

linux: openvpn3-core/.git
	mkdir -p $(BUILD_DIR)
	cmake -S linux -B $(BUILD_DIR) \
		-G Ninja \
		$(if $(CCACHE_BIN),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN)) \
		$(if $(LINKER),-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=$(notdir $(LINKER))") \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX=$(CMAKE_INSTALL_PREFIX) \
		-DCMAKE_INSTALL_LIBDIR=lib
	cmake --build $(BUILD_DIR) --target openlawsvpn-cli openlawsvpn
	cmake --install $(BUILD_DIR)
	@echo "Binary path: $(CMAKE_INSTALL_PREFIX)/bin/openlawsvpn-cli"
	@echo "Library path: $(CMAKE_INSTALL_PREFIX)/lib/libopenlawsvpn.so"

# GTK4 + libadwaita GUI (gui-gtk/ directory, Rust).
# Requires: libgtk4-devel, libadwaita-devel, cargo
gui: linux
	@if [ ! -d gui-gtk ]; then \
		echo "gui-gtk/ not found — run 'make gui-check' or see tasks/07_linux_gui/overview.md"; \
		exit 1; \
	fi
	cargo build --release --manifest-path gui-gtk/Cargo.toml
	mkdir -p $(GUI_BUILD_DIR)
	cp gui-gtk/target/release/openlawsvpn-gui $(GUI_BUILD_DIR)/
	@echo "GUI binary: $(GUI_BUILD_DIR)/openlawsvpn-gui"

gui-check:
	@echo "GUI status: gui-gtk/ directory not yet created."
	@echo "See tasks/07_linux_gui/overview.md for the implementation plan."

CONTAINER_ENGINE ?= podman
STATIC_BUILDER_IMAGE ?= localhost/openlawsvpn-static-builder
# Override to cross-build: make linux-static ARCH=arm64  (requires qemu-user-static)
ARCH ?= $(shell uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')

linux-static: openvpn3-core/.git
	$(CONTAINER_ENGINE) build --platform linux/$(ARCH) -t $(STATIC_BUILDER_IMAGE) -f linux/Containerfile.static .
	$(CONTAINER_ENGINE) run --rm --platform linux/$(ARCH) -v $(shell pwd):/src:z $(STATIC_BUILDER_IMAGE) \
		cmake -S linux -B build/linux-static -G Ninja \
		    -DCMAKE_BUILD_TYPE=Release \
		    -DCMAKE_INSTALL_PREFIX=build \
		    -DCMAKE_INSTALL_LIBDIR=lib \
		    -DBUILD_STATIC=ON \
		    -DENABLE_DBUS=OFF
	$(CONTAINER_ENGINE) run --rm --platform linux/$(ARCH) -v $(shell pwd):/src:z $(STATIC_BUILDER_IMAGE) \
		cmake --build build/linux-static --target openlawsvpn-cli-static
	@echo "Static binary: $(BUILD_DIR_STATIC)/openlawsvpn-cli-static"

clean:
	rm -rf build rpmbuild cmake-build-debug .ccache rpm-results
	rm -rf $(PROJECTTMPDIR)

linux-rpm:
	rpkg build --spec $(SPEC_FILE) --outdir rpmbuild
	@echo "RPMs are available in rpmbuild/RPMS/"
	@find rpmbuild/RPMS -name "*.rpm"

linux-srpm:
	mkdir -p $(PROJECTTMPDIR)
	spectool --get-files --directory $(PROJECTTMPDIR) $(SPEC_FILE)
	rpkg srpm --spec $(SPEC_FILE) --outdir $(PROJECTTMPDIR)

linux-rpm-mock: linux-srpm
	mock --no-clean -r fedora-$(FEDORA_VERSION)-x86_64 \
		$(PROJECTTMPDIR)/$(PROJECTNAME)-$(RPM_VERSION).src.rpm

openvpn3-rpm:
	mkdir -p $(PROJECTTMPDIR)
	spectool --get-files --directory $(PROJECTTMPDIR) docs/openvpn3.spec
	rpkg srpm --outdir $(PROJECTTMPDIR) --spec docs/openvpn3.spec
	mock --no-clean -r fedora-$(FEDORA_VERSION)-x86_64 \
        --addrepo=https://download.copr.fedorainfracloud.org/results/vorona/openlawsvpn/fedora-$(FEDORA_VERSION)-x86_64 \
		$(PROJECTTMPDIR)/openvpn3-$(OPENVPN3_RPM_VERSION).src.rpm
	@find ~/rpmbuild/RPMS /var/lib/mock/fedora-$(FEDORA_VERSION)-x86_64/result -name "openvpn3-*.rpm"

# Update openvpn3-core submodule to match a given openvpn3-linux release.
# Usage: make update-openvpn3-core OPENVPN3_LINUX_VERSION=v27
update-openvpn3-core:
	@eval $$(./scripts/resolve-openvpn3-core.sh $(OPENVPN3_LINUX_VERSION)) && \
		git -C openvpn3-core checkout $$OPENVPN3_CORE_SHA && \
		git add openvpn3-core && \
		echo "openvpn3-core updated to $$OPENVPN3_CORE_TAG ($$OPENVPN3_CORE_SHA)"
