#!/usr/bin/env bash
# Runs the iOS implementation (ios/RNZeroconf) on macOS against stub React headers,
# with AddressSanitizer, publishing and browsing real services through the local mDNSResponder.
# Usage: test/ios/run.sh
set -euo pipefail

cd "$(dirname "$0")/../.."
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

compile() {
  xcrun clang -g -fobjc-arc -fsanitize=address -Wall -Wno-deprecated-declarations \
    -framework Foundation -framework Network \
    -I test/ios -I ios/RNZeroconf \
    ios/RNZeroconf/RNZeroconf.m ios/RNZeroconf/RNNetServiceSerializer.m "$1" -o "$2"
}

compile test/ios/harness.m "$BUILD/harness"
compile test/ios/local-network-access.m "$BUILD/local-network-access"

echo "== harness"
"$BUILD/harness"
echo "== checkLocalNetworkAccess"
"$BUILD/local-network-access"
