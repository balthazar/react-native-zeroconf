# The C++ module on Android, built into the app's libappmodules.so by React Native's autolinking
# (react-native.config.js): the module, the dns_sd backend, and the mDNSResponder embedded for DNSSD
cmake_minimum_required(VERSION 3.13)
project(react-native-zeroconf)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(MDNS ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jni/mdnsresponder)

# The embedded mDNSResponder, with the flags of the standalone build it comes from
add_library(rnzeroconf_mdns STATIC
  ${MDNS}/mDNSCore/mDNS.c
  ${MDNS}/mDNSCore/DNSDigest.c
  ${MDNS}/mDNSCore/uDNS.c
  ${MDNS}/mDNSCore/DNSCommon.c
  ${MDNS}/mDNSPosix/mDNSPosix.c
  ${MDNS}/mDNSPosix/mDNSUNP.c
  ${MDNS}/mDNSPosix/PosixDaemon.c
  ${MDNS}/mDNSShared/mDNSDebug.c
  ${MDNS}/mDNSShared/dnssd_clientlib.c
  ${MDNS}/mDNSShared/dnssd_clientshim.c
  ${MDNS}/mDNSShared/dnssd_ipc.c
  ${MDNS}/mDNSShared/GenLinkedList.c
  ${MDNS}/mDNSShared/PlatformCommon.c
  ${MDNS}/mDNSShared/uds_daemon.c
)
target_compile_definitions(rnzeroconf_mdns PRIVATE
  _GNU_SOURCE HAVE_IPV6 HAVE_LINUX NOT_HAVE_SA_LEN PLATFORM_NO_RLIMIT TARGET_OS_LINUX USES_NETLINK
  MDNS_DEBUGMSGS=0 MDNS_UDS_SERVERPATH="/dev/socket/mdnsd" MDNS_USERNAME="mdnsr"
  SO_REUSEADDR UNICAST_DISABLED MDNS_VERSIONSTR_NODTS=1 EMBEDDED
)
set_source_files_properties(${MDNS}/mDNSPosix/PosixDaemon.c PROPERTIES
  # The standalone daemon's main, unused here
  COMPILE_DEFINITIONS main=mdnsd_main
)
target_compile_options(rnzeroconf_mdns PRIVATE -Os -fno-strict-aliasing -fwrapv -w)
target_include_directories(rnzeroconf_mdns
  PUBLIC ${MDNS}/mDNSShared
  PRIVATE ${MDNS}/mDNSPosix ${MDNS}/mDNSCore ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jni
)
target_link_libraries(rnzeroconf_mdns log)

add_library(react-native-zeroconf STATIC
  ${ROOT}/cpp/Backend.cpp
  ${ROOT}/cpp/ZeroconfModule.cpp
  ${ROOT}/cpp/dnssd/DnssdBackend.cpp
  ${ROOT}/cpp/embedded/EmbeddedExecutor.cpp
  ${ROOT}/cpp/android/ZeroconfAndroidModule.cpp
)
set_target_properties(react-native-zeroconf PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
# The autolinked module provider includes <ZeroconfAndroidModule.h>
target_include_directories(react-native-zeroconf PUBLIC ${ROOT}/cpp ${ROOT}/cpp/android)
target_link_libraries(react-native-zeroconf
  jsi
  reactnative
  fbjni
  react_codegen_RNZeroconfSpec
  rnzeroconf_mdns
  log
)
