# Installation on Windows

The library includes a Windows module for react-native-windows (C++, using the DNS-SD functions of Windows). Add Windows to your app with react-native-windows as usual, then install the library: autolinking picks up its `windows` project.

```bash
npm install react-native-windows
npx @react-native-community/cli init-windows --template cpp-app --overwrite
npm install react-native-zeroconf
npx @react-native-community/cli run-windows
```

> Tested in CI with react-native-windows 0.84 (New Architecture). Building needs the Windows SDK react-native-windows asks for (10.0.22621), see the [react-native-windows requirements](https://microsoft.github.io/react-native-windows/docs/rnw-dependencies).

Windows asks for no permission. A few features differ from the other platforms (publishing subtypes, `updateService()`), see [Platform Support](Platform-Support).
