# react-native-zeroconf [![Week Stars](http://starveller.sigsev.io/api/repos/Apercu/react-native-zeroconf/badge)](http://starveller.sigsev.io/Apercu/react-native-zeroconf)

> Basic Zeroconf implementation for React-native

Get running services advertizing themselves using Zeroconf implementations like Avahi, Bonjour or NSD.

### Install

    npm i -S react-native-zeroconf
    react-native install react-native-zeroconf
    react-native link react-native-zeroconf

You can look at [the wiki](https://github.com/Apercu/react-native-zeroconf/wiki) if you prefer a manual install.

TXT records will be available on iOS and Android >= 7.

### API

```javascript
import Zeroconf from 'react-native-zeroconf'
const zeroconf = new Zeroconf()
```

##### Methods

###### `scan(type = 'http', protocol = 'tcp', domain = 'local.')` Start the zeroconf scan

This will initialize the scan from the `Zeroconf` instance. Will stop another scan if any is running.

###### `stop()` Stop the scan

If any scan is running, stop it. Otherwise do nothing. Be sure to do this when the apps go to background, avoids potential memory leaks.

###### `getServices()` Returns resolved services

Will return all names of services that have been found or resolved.

###### `getResolvedServices()` Returns resolved services

Will return all names of services that have been fully resolved.

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

###### `update` 
Triggered either when a service is found, resolved or removed. It returns the service that has been updated. 

###### `error` Triggered when an error occurs
