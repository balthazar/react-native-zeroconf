# Installation

```bash
yarn add react-native-zeroconf
# or
npm install react-native-zeroconf
```

The library is a C++ native module, autolinked by React Native: there is nothing to link by hand. It needs the New Architecture, the only one since React Native 0.82. Rebuild the app after installing it: a JavaScript reload is not enough after adding a native module.

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
| React Native | 0.82, New Architecture (peer dependency `>=0.82`) |
| iOS | 13.4 or React Native's minimum |
| macOS | 10.15 |
| tvOS | 13.4 |
| Android | React Native's minimum (API 24 in React Native 0.86) |
| Windows | Windows 10, react-native-windows 0.84 tested |

See [Platform Support](Platform-Support) for feature differences.

## TypeScript

Types are bundled (`index.d.ts`). Remove `@types/react-native-zeroconf` if you installed it, since it conflicts with the bundled types.

```typescript
import Zeroconf, { ImplType, Service, ZeroconfError } from 'react-native-zeroconf'
```

## Next step

Configure [Permissions and Setup](Permissions-and-Setup): on iOS 14+ discovery fails until `NSBonjourServices` lists your service types.
