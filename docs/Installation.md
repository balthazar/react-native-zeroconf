# Installation

```bash
yarn add react-native-zeroconf
# or
npm install react-native-zeroconf
```

> **1.0 release candidate:** one C++ native module on every platform, for React Native 0.82 and later (New Architecture, Expo SDK 55 and later). Try it with `npm install react-native-zeroconf@next`, see [what changes from 0.17](Migrating-to-1.0). These docs cover 0.17, which stays the default and keeps getting bug fixes.

The library contains native code and is autolinked by React Native (0.60 and later), there is nothing to link by hand. Rebuild the app after installing it: a JavaScript reload is not enough after adding a native module.

## Your platform

| Platform | Page |
| --- | --- |
| iOS, macOS, tvOS | [iOS](Installation-iOS) |
| Android | [Android](Installation-Android) |
| Windows (react-native-windows) | [Windows](Installation-Windows) |
| Expo (development builds) | [Expo](Installation-Expo) |

## Requirements

| Platform | Minimum |
| --- | --- |
| React Native | 0.60 (peer dependency `>=0.60`) |
| iOS | 13.4 or React Native's minimum |
| macOS | 10.15 |
| tvOS | 13.4 |
| Android | API 21 (Android 5.0) |
| Windows | Windows 10, react-native-windows 0.84 tested |

See [Platform Support](Platform-Support) for feature differences.

## TypeScript

Types are bundled (`index.d.ts`). Remove `@types/react-native-zeroconf` if you installed it, since it conflicts with the bundled types.

```typescript
import Zeroconf, { ImplType, Service, ZeroconfError } from 'react-native-zeroconf'
```

## Next step

Configure [Permissions and Setup](Permissions-and-Setup): on iOS 14+ discovery fails until `NSBonjourServices` lists your service types.
