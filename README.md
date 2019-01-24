# react-native-zeroconf

> Basic Zeroconf implementation for React-native

Get running services advertizing themselves using Zeroconf implementations like Avahi, Bonjour or NSD.

### Install

    yarn add react-native-zeroconf
    react-native link

You can look at [the wiki](https://github.com/Apercu/react-native-zeroconf/wiki) if you prefer a manual install.

TXT records will be available on iOS and Android >= 7.

For Android please ensure your manifest is requesting all necessary permissions.

```xml
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
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
