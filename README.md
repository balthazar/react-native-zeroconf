# react-native-zeroconf

> Basic Zeroconf implementation for React-native

Get running services advertizing themselves using Zeroconf implementations like Avahi, Bonjour or NSD.

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
  "addresses": [
    "192.168.1.23",
    "fe80::aebc:123:ffff:abcd"
  ],
  "name": "Xerox Printer",
  "fullName": "XeroxPrinter.local._http._tcp.",
  "port": 8080
}
```

###### `remove` Triggered when a service is removed

Broadcast a service name removed from the network.

###### `update` Triggered either when a service is found or removed
###### `error` Triggered when an error occurs
