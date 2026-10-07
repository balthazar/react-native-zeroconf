// swift-tools-version: 6.0
import PackageDescription

// Swift Package Manager support for React Native's SwiftPM autolinking
// (react-native >= 0.87). CocoaPods users are unaffected; see the podspec.
// The two React Native packages are resolved by the autolinker relative to
// build/generated/autolinking/libs/ReactNativeZeroconf in the app.
let package = Package(
    name: "ReactNativeZeroconf",
    platforms: [.iOS(.v15)],
    products: [
        .library(name: "ReactNativeZeroconf", targets: ["ReactNativeZeroconf"]),
    ],
    dependencies: [
        .package(name: "ReactNative", path: "../../../../xcframeworks"),
        .package(name: "React-GeneratedCode", path: "../../../ios"),
    ],
    targets: [
        .target(
            name: "ReactNativeZeroconf",
            dependencies: [
                .product(name: "ReactHeaders", package: "ReactNative"),
                .product(name: "ReactNativeHeaders", package: "ReactNative"),
                .product(name: "ReactNativeDependenciesHeaders", package: "ReactNative"),
                .product(name: "ReactAppHeaders", package: "React-GeneratedCode"),
            ],
            path: "ios/RNZeroconf",
            publicHeadersPath: ".",
            linkerSettings: [
                .linkedFramework("Foundation"),
                .linkedFramework("Network"),
            ]
        ),
    ]
)
