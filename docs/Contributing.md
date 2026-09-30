# Contributing

Bug reports and pull requests are welcome on [GitHub](https://github.com/balthazar/react-native-zeroconf).

## Repository layout

| Path | Contents |
| --- | --- |
| `src/index.js` | JavaScript API (compiled to `dist/` with Babel) |
| `index.d.ts` | TypeScript definitions, keep in sync with `src/index.js` |
| `ios/RNZeroconf/` | iOS, macOS and tvOS module (Objective-C, dns_sd) |
| `react-native-zeroconf.podspec` | CocoaPods spec |
| `android/src/main/java/com/balthazargronon/RCTZeroconf/` | Android module (`nsd/` and `rx2dnssd/` implementations) |
| `android/src/main/java/com/github/druk/` | Bundled RxDNSSD Java sources |
| `android/src/main/jni/` | Embedded mDNSResponder, built with `ndkBuild` |
| `example/` | React Native example app |

## JavaScript

```bash
yarn install
yarn lint      # ESLint on src/
yarn format    # Prettier on src/
yarn build     # Babel: src/ -> dist/
```

## Example app

```bash
cd example
yarn install
cd ios && pod install && cd ..   # required before the first iOS build, and after native changes
yarn ios        # or: yarn android
```

> Run `pod install` again whenever you change the podspec or add or remove iOS source files.

<!-- TODO: the example's package.json depends on a published react-native-zeroconf version. Document how to point it at the local checkout (for example a file: or link: dependency) to test unreleased changes. -->

## Android

The Android module compiles the embedded mDNSResponder with the NDK, so the NDK must be installed (the module defaults to `ndkVersion` 27.1.12297006 unless the app overrides it through `rootProject.ext.ndkVersion`).

```bash
cd example/android
./gradlew assembleDebug
```

## Testing

- mDNS does not work on the Android emulator by default: test Android on a real device ([Android Emulator](Android-Emulator)).
- On iOS, test on a device or simulator with Local Network access, and add the service types you test to the example's `NSBonjourServices`.
- Useful tools on a Mac: `dns-sd -B _http._tcp` to browse, `dns-sd -R "Test" _http._tcp local 8080 path=/` to publish a test service.
- For Android changes, test both `implType: 'NSD'` and `'DNSSD'`, and if possible Android 14+ and an older version (the `NSD` code paths differ).

## Documentation

The documentation lives in `docs/` as Markdown. It is published at [zeroconf.balthazar.dev](https://zeroconf.balthazar.dev) and copied to the GitHub wiki as a backup.

- Preview the site: `python3 website/build.py docs website/dist/index.html`, then open `website/dist/index.html` through any static server.
- Deploy the site: `website/deploy.sh` (needs access to the cluster).
- Update the wiki backup: `website/sync-wiki.sh`.

## Pull requests

- Keep `README.md`, `index.d.ts` and `docs/` consistent with API changes.
- Describe the platforms and OS versions you tested on.
