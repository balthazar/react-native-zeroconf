#!/usr/bin/env bash
# Runs the dns_sd backend (cpp/dnssd) on macOS with AddressSanitizer, publishing and browsing real services
# through the system mDNSResponder, then the Local Network check (cpp/apple/LocalNetworkAccess.mm).
# Usage: test/apple/run.sh
set -euo pipefail

cd "$(dirname "$0")/../.."
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

xcrun clang++ -std=c++20 -g -fsanitize=address -Wall -Wextra \
  cpp/Backend.cpp cpp/dnssd/DnssdBackend.cpp cpp/apple/DispatchExecutor.cpp test/native/harness.cpp \
  -o "$BUILD/harness"
xcrun clang++ -std=c++20 -g -fobjc-arc -fsanitize=address -Wall -Wextra -framework Foundation -framework Network \
  cpp/Backend.cpp cpp/apple/LocalNetworkAccess.mm test/apple/local-network-access.mm \
  -o "$BUILD/local-network-access"

echo "== dns_sd backend"
"$BUILD/harness"
echo "== Local Network check"
"$BUILD/local-network-access"
