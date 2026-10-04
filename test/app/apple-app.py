"""Prepares an app made from the react-native-macos or react-native-tvos template for the self-test:
the Bonjour service types it uses, the network entitlements on macOS, and the template's AppDelegate."""
import pathlib
import plistlib
import sys

app = pathlib.Path(sys.argv[1])

info = app / "Info.plist"
plist = plistlib.loads(info.read_bytes())
plist["NSBonjourServices"] = ["_zcpeer._tcp", "_zcghost._tcp", "_zcapp._tcp", "_zcresult._tcp", "_services._dns-sd._udp"]
plist["NSLocalNetworkUsageDescription"] = "Zeroconf self-test"
info.write_bytes(plistlib.dumps(plist))

# The macOS app is sandboxed: publishing needs the server entitlement
for entitlements in app.glob("*.entitlements"):
    plist = plistlib.loads(entitlements.read_bytes())
    plist["com.apple.security.network.client"] = True
    plist["com.apple.security.network.server"] = True
    entitlements.write_bytes(plistlib.dumps(plist))

# The react-native-macos 0.83 template calls super.init() before setting its properties, which Swift rejects
delegate = app / "AppDelegate.swift"
source = delegate.read_text()
if "  override init() {\n    super.init()\n\n" in source:
    source = source.replace("  override init() {\n    super.init()\n\n", "  override init() {\n")
    source = source.replace("    reactNativeFactory = factory\n  }", "    reactNativeFactory = factory\n    super.init()\n  }")
    delegate.write_text(source)
