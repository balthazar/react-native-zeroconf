# Installation on iOS

After [installing the package](Installation), install the pod:

```bash
cd ios && pod install
```

Then rebuild the app (`yarn ios` or from Xcode). A JavaScript reload is not enough after adding a native module.

Before scanning, declare your service types in `NSBonjourServices` and add `NSLocalNetworkUsageDescription`, see [Permissions and Setup](Permissions-and-Setup#ios). Discovery fails on iOS 14+ without them.

## macOS (react-native-macos)

```bash
cd macos && pod install
```

macOS 10.15 or later.

## tvOS

The podspec supports tvOS 13.4 or later, `pod install` in your tvOS project as on iOS.
