# react-native-zeroconf

> Basic Zeroconf implementation for React-native

Get running services advertizing themselves using Zeroconf implementations like Avahi, Bonjour or NSD.

## Android 16KB Page Size Support

This fork includes bundled native code from [Discord's RxDNSSD fork](https://github.com/discord/RxDNSSD) with 16KB page size alignment support, required for Android 15+ devices (Google Play requirement starting November 1, 2025).

The native DNS-SD implementation is built directly into this library with the embedded mDNSResponder (`Rx2DnssdEmbedded`), which:

- Works reliably across **all Android versions** (5.0+)
- Eliminates external AAR dependencies
- Provides consistent behavior regardless of device manufacturer or Android variant
- Does not depend on the system mDNS daemon (which doesn't exist on most devices)

### Install

    yarn add react-native-zeroconf
    # for react-native < 0.60 only (all platforms):
    react-native link
    # for ios (when using CocoaPods):
    (cd ios && pod install)
    # for macOS (when using CocoaPods):
    (cd macos && pod install)

You can look at [the wiki](https://github.com/Apercu/react-native-zeroconf/wiki) if you prefer a manual install.

TXT records will be available on iOS and Android >= 7.

For Android please ensure your manifest is requesting all necessary permissions.

```xml
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

### Android

Supports all Android versions from 5.0 (API 21) onwards using the embedded mDNSResponder implementation.

### Android Emulator Limitations

**Important:** The Android emulator does not support IGMP or multicast. This is a [documented limitation](https://developer.android.com/studio/run/emulator-networking) that affects **all Android emulator versions**.

Since mDNS/Bonjour relies on multicast UDP packets to `224.0.0.251:5353`, Zeroconf discovery **will not work on Android emulators** by default.

#### Workarounds for Emulator Testing

**Option 1: Use a Real Device (Recommended)**

For reliable mDNS testing, use a physical Android device connected to the same network as the services you want to discover.

**Option 2: Enable Bridged Networking (Linux)**

On Linux, you can configure TAP bridged networking to allow the emulator to receive multicast packets:

1. Install required tools:
   ```bash
   sudo apt-get install bridge-utils
   ```

2. Create TAP interface and bridge:
   ```bash
   # Create TAP interface
   sudo ip tuntap add dev tap0 mode tap user $USER

   # Get your main network interface name
   ip link show  # (e.g., eth0, enp3s0)

   # Create bridge and add interfaces
   sudo ip link add name br0 type bridge
   sudo ip link set enp3s0 master br0  # Replace with your interface
   sudo ip link set tap0 master br0

   # Bring up interfaces
   sudo ip link set dev tap0 up
   sudo ip link set dev br0 up

   # Get IP via DHCP
   sudo dhclient br0
   ```

3. Launch emulator with TAP networking:
   ```bash
   emulator -avd <avd_name> -net-tap tap0
   ```

4. In the emulator, toggle Airplane Mode on/off to refresh network configuration.

**Option 3: ADB Port Forwarding (Direct Connections Only)**

For direct TCP connections (not mDNS discovery):
```bash
adb reverse tcp:9100 tcp:192.168.1.100:9100
```

Then connect to `localhost:9100` in your app.

**Note:** WiFi bridging is complex because most WiFi cards don't support bridging. Use Ethernet for TAP bridged networking, or consider [Genymotion](https://www.genymotion.com/) which supports bridged networking by default.

### IOS 14 Permissions

IOS 14 requires you to specify the services you want to scan for and a description for what you're using them.

In your `info.plist` add the following strings:

```xml
<key>NSBonjourServices</key>
	<array>
		<string>my_service._tcp.</string>
		<string>my_other_service._tcp.</string>
	</array>
<key>NSLocalNetworkUsageDescription</key>
<string>Describe why you want to use local network discovery here</string>
```

### Example

Take a look at the [example folder](./example). Install the dependencies, run `node server.js` and launch the project.

### API

```javascript
import Zeroconf from 'react-native-zeroconf'
const zeroconf = new Zeroconf()
```

##### Methods

###### `scan(type = 'http', protocol = 'tcp', domain = 'local.')` Start the zeroconf scan

This will initialize the scan from the `Zeroconf` instance. Will stop another scan if any is running.

###### `stop()` Stop the scan

If any scan is running, stop it. Otherwise do nothing.

###### `getServices()` Returns resolved services

Will return all names of services that have been resolved.

###### `removeDeviceListeners()` Remove listeners

Allow you to clean the listeners, avoiding potential memory leaks ([#33](https://github.com/Apercu/react-native-zeroconf/issues/33)).

###### `addDeviceListeners()` Add listeners

If you cleaned the listeners and need to get them back on.

###### `publishService(type, protocol, domain, name, port, txt)` Publish a service

This adds a service for the current device to the discoverable services on the network.

`domain` should be the domain the service is sitting on, dot suffixed, for example `'local.'`
`type` should be both type and protocol, underscore prefixed, for example `'_http._tcp'`
`name` should be unique to the device, often the device name
`port` should be an integer
`txt` should be a hash, for example `{"foo": "bar"}`

###### `unpublishService(name)` Unpublish a service

This removes a service from those discoverable on the network.

`name` should be the name used when publishing the service

##### Events

```javascript
zeroconf.on('start', () => console.log('The scan has started.'))
```

###### `start` Triggered on scan start

###### `stop` Triggered on scan stop

###### `found` Triggered when a service is found

Broadcast a service name as soon as it is found.

###### `resolved` Triggered when a service is resolved

Broadcast a service object once it is fully resolved

```json
{
  "host": "XeroxPrinter.local.",
  "addresses": ["192.168.1.23", "fe80::aebc:123:ffff:abcd"],
  "name": "Xerox Printer",
  "fullName": "XeroxPrinter.local._http._tcp.",
  "port": 8080
}
```

###### `remove` Triggered when a service is removed

Broadcast a service name removed from the network.

###### `update` Triggered either when a service is found or removed

###### `error` Triggered when an error occurs

### License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

#### Third-Party Licenses

This project includes code from [RxDNSSD](https://github.com/discord/RxDNSSD) (originally by Andriy Druk, maintained by Discord), which is licensed under the Apache License 2.0. See the [NOTICE](NOTICE) file for details.
