#!/usr/bin/env bash
# Runs the dns_sd backend (cpp/dnssd) on Linux against the mDNSResponder embedded for Android DNSSD,
# built from the module's sources with the flags of android/src/main/jni/Android.mk, with AddressSanitizer.
# Needs clang. On macOS: docker run --rm -v "$PWD":/src -w /src ubuntu:24.04 bash -c "apt-get update -qq && apt-get install -yqq clang >/dev/null && test/embedded/run.sh"
# Usage: test/embedded/run.sh
set -euo pipefail

cd "$(dirname "$0")/../.."
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

JNI=android/src/main/jni
MDNS=$JNI/mdnsresponder
SOURCES=(
  mDNSCore/mDNS.c mDNSCore/DNSDigest.c mDNSCore/uDNS.c mDNSCore/DNSCommon.c
  mDNSPosix/mDNSPosix.c mDNSPosix/mDNSUNP.c mDNSPosix/PosixDaemon.c
  mDNSShared/mDNSDebug.c mDNSShared/dnssd_clientlib.c mDNSShared/dnssd_clientshim.c mDNSShared/dnssd_ipc.c
  mDNSShared/GenLinkedList.c mDNSShared/PlatformCommon.c mDNSShared/uds_daemon.c
)
CFLAGS=(
  -g -O1 -fsanitize=address -fno-omit-frame-pointer -fno-strict-aliasing -fwrapv
  -D_GNU_SOURCE -DHAVE_IPV6 -DHAVE_LINUX -DNOT_HAVE_SA_LEN -DPLATFORM_NO_RLIMIT -DTARGET_OS_LINUX -DUSES_NETLINK
  -DMDNS_DEBUGMSGS=0 '-DMDNS_UDS_SERVERPATH="/dev/socket/mdnsd"' '-DMDNS_USERNAME="mdnsr"'
  -DSO_REUSEADDR -DUNICAST_DISABLED -DMDNS_VERSIONSTR_NODTS=1 -DEMBEDDED
  -w -I $MDNS/mDNSPosix -I $MDNS/mDNSCore -I $MDNS/mDNSShared -I $JNI
)

for source in "${SOURCES[@]}"; do
  # PosixDaemon.c also has the standalone daemon's main, unused in Android's shared library
  clang "${CFLAGS[@]}" -Dmain=mdnsd_main -c "$MDNS/$source" -o "$BUILD/$(basename "$source" .c).o"
done
clang++ -std=c++20 -g -O1 -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra \
  -I $MDNS/mDNSShared \
  cpp/Backend.cpp cpp/dnssd/DnssdBackend.cpp cpp/embedded/EmbeddedExecutor.cpp test/native/harness.cpp \
  "$BUILD"/*.o -lpthread -o "$BUILD/harness"

LSAN_OPTIONS="suppressions=test/embedded/lsan.supp" "$BUILD/harness"
