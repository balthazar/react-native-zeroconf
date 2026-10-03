#!/usr/bin/env bash
# Runs the dns_sd backend (cpp/dnssd) on macOS with AddressSanitizer, publishing and browsing real services
# through the system mDNSResponder.
# Usage: test/apple/run.sh
set -euo pipefail

cd "$(dirname "$0")/../.."
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

xcrun clang++ -std=c++20 -g -fsanitize=address -Wall -Wextra \
  cpp/Backend.cpp cpp/dnssd/DnssdBackend.cpp cpp/apple/DispatchExecutor.cpp test/apple/harness.cpp \
  -o "$BUILD/harness"

"$BUILD/harness"
