// swift-tools-version: 6.0
import PackageDescription

// Swift Package Manager support for React Native's SwiftPM autolinking
// (react-native >= 0.87). CocoaPods users are unaffected; see the podspec.
// The two React Native packages are resolved by the autolinker relative to
// build/generated/autolinking/libs/ReactNativeZeroconf in the app.
// Same sources as the podspec: the C++ module, its Apple registration and the
// codegen output shipped in ios/generated.
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
            path: ".",
            sources: [
                "cpp/Backend.cpp",
                "cpp/ZeroconfModule.cpp",
                "cpp/apple",
                "cpp/dnssd",
                "ios",
            ],
            publicHeadersPath: "ios/generated/ReactCodegen",
            cxxSettings: [
                .headerSearchPath("ios/generated/ReactCodegen"),
                // Match the NDEBUG-gated C++ ABI of the prebuilt React Native frameworks
                .define("DEBUG", .when(configuration: .debug)),
                .define("NDEBUG", .when(configuration: .release)),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
                .linkedFramework("Network"),
            ]
        ),
    ],
    cxxLanguageStandard: .cxx20
)
