# Installation on iOS

After [installing the package](Installation), install the pod:

```bash
cd ios && pod install
```

Then rebuild the app (`yarn ios` or from Xcode). A JavaScript reload is not enough after adding a native module.

Before scanning, declare your service types in `NSBonjourServices` and add `NSLocalNetworkUsageDescription`, see [Permissions and Setup](Permissions-and-Setup#ios). Discovery fails on iOS 14+ without them.

## Swift Package Manager

React Native 0.87 and later can autolink with Swift Package Manager instead of CocoaPods. The package ships a `Package.swift` for it, so nothing else is needed: run `npx react-native spm update` after installing and rebuild. iOS 15 or later.

## macOS (react-native-macos)

```bash
cd macos && pod install
```

macOS 10.15 or later.

## tvOS

The podspec supports tvOS 13.4 or later, `pod install` in your tvOS project as on iOS.
