# react-native-zeroconf [![Week Stars](http://starveller.sigsev.io/api/repos/Apercu/react-native-zeroconf/badge)](http://starveller.sigsev.io/Apercu/react-native-zeroconf)

> Basic Zeroconf implementation for React-native

Get running services advertizing themselves using Zeroconf implementations like Avahi, Bonjour or NSD.

### Install

    npm i -S react-native-zeroconf

##### iOS

 - Right click on the `Libraries` folder in XCode, and add `RNZeroconf.xcodeproj`
 - Go to your *Build Phases*, under *Link Binary with Libraries*, add `libRNZeroconf.a`
 - Click on the `RNZeroconf.xcodeproj` in the `Libraries` folder, search *Header Search Paths* and add `$(SRCROOT)/../../react-native/React` if it's not.

##### Android

 - Add the following line to the bottom of your project's `settings.gradle` file.

    `project(':react-native-zeroconf').projectDir = new File(settingsDir, '../node_modules/react-native-zeroconf/android')`

 - Change the `include` line of your project's `settings.gradle` to include the `:react-native-zeroconf` project. Example:

    `include ':react-native-zeroconf', ':app'`

 - Open your app's `build.gradle` file and add the following line to the `dependencies` block.

    `compile project(":react-native-zeroconf")`

 - In your app's `MainActivity.java` file, include this line as part of the `ReactInstanceManager.builder()` lines.

    `.addPackage(new ZeroconfReactPackage())`

### API

    import Zeroconf from 'react-native-zeroconf';
    const zeroconf = new Zeroconf();

##### Methods

###### `scan(type = 'http', protocol = 'tcp', domain = 'local.')` Start the zeroconf scan

This will initialize the scan from the `Zeroconf` instance. Will stop another scan if any is running.

###### `stop()` Stop the scan

If any scan is running, stop it. Otherwise do nothing.

###### `getServices()` Returns resolved services

Will return all names of services that have been resolved.

##### Events

    zeroconf.on('start', () => { console.log('The scan has started.'); });

###### `start` Triggered on scan start
###### `stop` Triggered on scan stop
###### `found` Triggered when a service is found

Broadcast a service name as soon as it is found.

###### `resolved` Triggered when a service is resolved

Broadcast a service object once it is fully resolved

    {
      host: 'XeroxPrinter.local.',
      addresses: [
        '192.168.1.23',
        'fe80::aebc:123:ffff:abcd'
      ],
      name: 'Xerox Printer',
      fullName: 'XeroxPrinter.local._http._tcp.',
      port: 8080
    }

###### `remove` Triggered when a service is removed

Broadcast a service name removed from the network.

###### `update` Triggered either when a service is found or removed
###### `error` Triggered when an error occurs

### Webpack

This component uses ES6. If using webpack you should launch `babel` on the `Zeroconf.js` file.
