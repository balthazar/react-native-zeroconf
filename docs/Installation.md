# Installation

The library contains native code and is autolinked by React Native (0.60 and later). There is nothing to link by hand.

## React Native CLI (bare)

```bash
yarn add react-native-zeroconf
# or
npm install react-native-zeroconf
```

### iOS

```bash
cd ios && pod install
```

Then rebuild the app (`yarn ios` or from Xcode). A JavaScript reload is not enough after adding a native module.

### macOS (react-native-macos)

```bash
cd macos && pod install
```

### Android

Nothing to do: Gradle picks the module up through autolinking, and the required permissions are merged from the library manifest (see [Permissions and Setup](Permissions-and-Setup#android)). Rebuild the app with `yarn android`.

### Windows (react-native-windows)

The library includes a Windows module (C++, using the DNS-SD functions of Windows). Add Windows to your app with react-native-windows as usual, then install the library: autolinking picks up its `windows` project.

```bash
npm install react-native-windows
npx @react-native-community/cli init-windows --template cpp-app --overwrite
npm install react-native-zeroconf
npx @react-native-community/cli run-windows
```

> Tested in CI with react-native-windows 0.84 (New Architecture). Building needs the Windows SDK react-native-windows asks for (10.0.22621), see the [react-native-windows requirements](https://microsoft.github.io/react-native-windows/docs/rnw-dependencies).

## Expo

The library works in Expo **development builds**. It does **not** work in **Expo Go**, which only contains the native modules bundled by Expo.

> Tested with Expo SDK 54 and the New Architecture.

```bash
npx expo install react-native-zeroconf
npx expo run:ios       # or: npx expo run:android, or an EAS Build
```

Declare the iOS keys in `app.json` so they are written to `Info.plist` on prebuild:

```json
{
  "expo": {
    "ios": {
      "infoPlist": {
        "NSBonjourServices": ["_http._tcp"],
        "NSLocalNetworkUsageDescription": "This app uses the local network to discover devices."
      }
    }
  }
}
```

No config plugin is needed. Android permissions come from the library manifest.

> If you see `react-native-zeroconf: native module not found`, you are running in Expo Go or the app was not rebuilt after installing. See [Troubleshooting and FAQ](Troubleshooting-and-FAQ#native-module-not-found-expo-go).

## TypeScript

Types are bundled (`index.d.ts`). Remove `@types/react-native-zeroconf` if you installed it, since it conflicts with the bundled types.

```typescript
import Zeroconf, { ImplType, Service, ZeroconfError } from 'react-native-zeroconf'
```

## Requirements

| Platform | Minimum |
| --- | --- |
| React Native | 0.60 (peer dependency `>=0.60`) |
| iOS | 13.4, or React Native's minimum if higher |
| macOS | 10.15 |
| tvOS | 13.4 |
| Android | API 21 (Android 5.0) |
| Windows | Windows 10 with react-native-windows (tested with 0.84) |

See [Platform Support](Platform-Support) for feature differences.

## Next step

Configure [Permissions and Setup](Permissions-and-Setup): on iOS 14+ discovery fails until `NSBonjourServices` lists your service types.
