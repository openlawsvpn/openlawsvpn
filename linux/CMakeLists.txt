# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2024 openlawsvpn contributors
# See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
cmake_minimum_required(VERSION 3.13)
project(openlawsvpn)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../openvpn3-core)
set(LINUX_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(SYSTEM ${CORE_DIR})
include_directories(${LINUX_DIR}/include)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(LZ4 REQUIRED liblz4)

# D-Bus support is optional
option(ENABLE_DBUS "Enable D-Bus support" ON)

if(ENABLE_DBUS)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    pkg_check_modules(GIO REQUIRED gio-2.0)
    set(DBUS_SOURCES src/dbus_client.cpp)
    set(DBUS_DEFINITIONS ENABLE_DBUS)
    set(DBUS_INCLUDE_DIRS ${GLIB_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS})
    set(DBUS_LIBRARIES ${GLIB_LIBRARIES} ${GIO_LIBRARIES})
else()
    set(DBUS_SOURCES "")
    set(DBUS_DEFINITIONS "")
    set(DBUS_INCLUDE_DIRS "")
    set(DBUS_LIBRARIES "")
    # If D-Bus is disabled, default to DIRECT mode
    set(OPENVPN_DEFAULT_MODE "DIRECT" CACHE STRING "Default connection mode (DBUS or DIRECT)" FORCE)
endif()

# Default mode: DBUS or DIRECT
set(OPENVPN_DEFAULT_MODE "DBUS" CACHE STRING "Default connection mode (DBUS or DIRECT)")

add_library(openlawsvpn SHARED
    src/libopenlawsvpn.cpp
    src/saml_capture.cpp
    src/libopenlawsvpn_ffi.cpp
    ${DBUS_SOURCES}
    ${CORE_DIR}/openvpn/crypto/data_epoch.cpp
)

target_compile_definitions(openlawsvpn PRIVATE
    ASIO_STANDALONE
    USE_ASIO
    HAVE_LZ4
    USE_OPENSSL
    OPENVPN_DEFAULT_MODE=\"${OPENVPN_DEFAULT_MODE}\"
    ${DBUS_DEFINITIONS}
)

target_include_directories(openlawsvpn PRIVATE
    ${DBUS_INCLUDE_DIRS}
)

target_link_libraries(openlawsvpn
    PRIVATE
    ${OPENSSL_LIBRARIES}
    ${LZ4_LIBRARIES}
    ${DBUS_LIBRARIES}
    pthread
)

# CLI tool
add_executable(openlawsvpn-cli
    src/main.cpp
)

target_compile_definitions(openlawsvpn-cli PRIVATE
    ASIO_STANDALONE
    USE_ASIO
)

target_link_libraries(openlawsvpn-cli
    PRIVATE
    openlawsvpn
    pthread
)

install(TARGETS openlawsvpn-cli DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS openlawsvpn
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

set_target_properties(openlawsvpn-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH "$ORIGIN/../lib"
)
