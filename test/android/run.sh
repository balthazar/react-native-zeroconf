#!/usr/bin/env bash
# Runs the embedded mDNSResponder of the Android module (android/src/main/jni) on Linux under
# AddressSanitizer, driving it from several threads the way the Java side does.
# Usage: test/android/run.sh
set -euo pipefail

cd "$(dirname "$0")/../.."
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

JNI=android/src/main/jni
MDNS=$JNI/mdnsresponder
CC="${CC:-cc}"

# The sources and flags of the jdns_sd_embedded module in android/src/main/jni/Android.mk,
# without JNISupport.c (it needs the JNI). Keep them in sync.
SOURCES=(
  "$MDNS/mDNSCore/mDNS.c"
  "$MDNS/mDNSCore/DNSDigest.c"
  "$MDNS/mDNSCore/uDNS.c"
  "$MDNS/mDNSPosix/mDNSPosix.c"
  "$MDNS/mDNSPosix/mDNSUNP.c"
  "$MDNS/mDNSShared/mDNSDebug.c"
  "$MDNS/mDNSShared/dnssd_clientlib.c"
  "$MDNS/mDNSShared/dnssd_clientshim.c"
  "$MDNS/mDNSShared/dnssd_ipc.c"
  "$MDNS/mDNSShared/GenLinkedList.c"
  "$MDNS/mDNSShared/PlatformCommon.c"
  "$MDNS/mDNSCore/DNSCommon.c"
  "$MDNS/mDNSShared/uds_daemon.c"
)
FLAGS=(
  -fwrapv -fno-strict-aliasing
  -D_GNU_SOURCE -DHAVE_IPV6 -DHAVE_LINUX -DNOT_HAVE_SA_LEN -DPLATFORM_NO_RLIMIT
  -DTARGET_OS_LINUX -DUSES_NETLINK -DMDNS_DEBUGMSGS=0
  -DMDNS_UDS_SERVERPATH='"/dev/socket/mdnsd"' -DMDNS_USERNAME='"mdnsr"'
  -DSO_REUSEADDR -DUNICAST_DISABLED -DMDNS_VERSIONSTR_NODTS=1 -DAUTO_CALLBACKS=1 -DEMBEDDED
  -I "$MDNS/mDNSPosix" -I "$MDNS/mDNSCore" -I "$MDNS/mDNSShared" -I "$JNI"
  -w -Werror=implicit-function-declaration
)
# The harness looks for memory errors: freed memory read by another thread shows up only here.
SANITIZE=(-g -O1 -fno-omit-frame-pointer -fsanitize=address -pthread)

OBJECTS=()
for source in "${SOURCES[@]}"; do
  object="$BUILD/$(basename "$source" .c).o"
  "$CC" "${FLAGS[@]}" "${SANITIZE[@]}" -c "$source" -o "$object"
  OBJECTS+=("$object")
done
# PosixDaemon.c keeps the daemon's main() next to the embedded init()/loop(); the harness has its own.
"$CC" "${FLAGS[@]}" "${SANITIZE[@]}" -Dmain=mdnsd_main -c "$MDNS/mDNSPosix/PosixDaemon.c" -o "$BUILD/PosixDaemon.o"
OBJECTS+=("$BUILD/PosixDaemon.o")
"$CC" "${FLAGS[@]}" "${SANITIZE[@]}" -c test/android/harness.c -o "$BUILD/harness.o"
"$CC" "${SANITIZE[@]}" "${OBJECTS[@]}" "$BUILD/harness.o" -o "$BUILD/harness"

echo "== embedded responder harness"
# mDNSResponder leaves a few allocations behind at exit, this is not what the harness is about.
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$BUILD/harness"
