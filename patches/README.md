# openlawsvpn Patches

This directory contains patches applied to the `openvpn3-core` source during the build process.

## Patch Details

### `cryptoalgs.hpp.patch`
- **Target:** `openvpn3-core/openvpn/crypto/cryptoalgs.hpp`
- **Purpose:** Fixes compilation warnings/errors related to implicit conversions between `unsigned int` and enum-based flags when using bitwise OR operations.
- **Upstream Status:** This is a compatibility fix for newer C++ compilers (C++20/C++23) where strict enum conversions are enforced. It is a candidate for upstreaming to `openvpn3-core`.

### `ovpncli.cpp.patch`
- **Target:** `openvpn3-core/client/ovpncli.cpp`
- **Purpose:** Explicitly casts enum values and constants to `size_t` to avoid narrowing or signed/unsigned comparison warnings.
- **Upstream Status:** Minor maintenance fix. Candidate for upstreaming to `openvpn3-core`.

## Why these patches?
The `openlawsvpn` project targets modern distributions (RHEL 9+, Fedora) with recent GCC/Clang versions. These patches ensure a clean, warning-free build of the underlying OpenVPN 3 core library.
