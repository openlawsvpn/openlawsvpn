#!/usr/bin/env bash
# Resolves the openvpn3-core commit/tag that corresponds to a given
# openvpn3-linux release via its git submodule reference.
#
# Usage:
#   ./scripts/resolve-openvpn3-core.sh v27
#
# Output (stdout, one line each):
#   OPENVPN3_LINUX_VERSION=v27
#   OPENVPN3_CORE_SHA=<full commit sha>
#   OPENVPN3_CORE_TAG=<tag name, if one exists>   # or empty if untagged

set -euo pipefail

OPENVPN3_LINUX_REPO="https://github.com/OpenVPN/openvpn3-linux.git"
OPENVPN3_CORE_REPO="https://github.com/OpenVPN/openvpn3.git"

VERSION="${1:-}"
if [[ -z "$VERSION" ]]; then
    echo "Usage: $0 <openvpn3-linux-version>" >&2
    echo "Example: $0 v27" >&2
    exit 1
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# Shallow clone openvpn3-linux at the given tag (no submodule checkout)
git -c advice.detachedHead=false clone --quiet --depth=1 --branch="$VERSION" \
    --no-recurse-submodules "$OPENVPN3_LINUX_REPO" "$TMPDIR/openvpn3-linux" 2>/dev/null

# Read the submodule commit SHA (openvpn3-core → https://github.com/OpenVPN/openvpn3)
CORE_SHA=$(git -C "$TMPDIR/openvpn3-linux" ls-tree HEAD openvpn3-core \
    | awk '{print $3}')

if [[ -z "$CORE_SHA" ]]; then
    echo "error: could not find openvpn3-core submodule in openvpn3-linux $VERSION" >&2
    exit 1
fi

# Find a matching tag in openvpn3-core (dereference annotated tags with ^{})
# Fetch all tag refs first, then search — avoids SIGPIPE from early awk exit
CORE_TAGS=$(git ls-remote --tags "$OPENVPN3_CORE_REPO" 2>/dev/null)
CORE_TAG=$(echo "$CORE_TAGS" | awk -v sha="$CORE_SHA" '
    /\^\{\}$/ { if ($1 == sha) { sub(/refs\/tags\//, "", $2); sub(/\^\{\}$/, "", $2); print $2; exit } }
    { if ($1 == sha) { sub(/refs\/tags\//, "", $2); tag=$2 } }
    END { if (tag) print tag }
')

echo "OPENVPN3_LINUX_VERSION=$VERSION"
echo "OPENVPN3_CORE_SHA=$CORE_SHA"
echo "OPENVPN3_CORE_TAG=${CORE_TAG:-}"
