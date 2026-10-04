# Installation with Expo

The library works in Expo **development builds**. It does **not** work in **Expo Go**, which only contains the native modules bundled by Expo.

> Needs an SDK with React Native 0.82 or later (SDK 55 and later). Tested with SDK 57 (React Native 0.86).

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

No config plugin is needed. Android permissions come from the library manifest. Apps targeting Android 17 (API 37) add the local network permission to `app.json`, see [Permissions and Setup](Permissions-and-Setup#android-17-local-network-permission):

```json
{
  "expo": {
    "android": {
      "permissions": ["android.permission.ACCESS_LOCAL_NETWORK"]
    }
  }
}
```

> If you see `react-native-zeroconf: native module not found`, you are running in Expo Go or the app was not rebuilt after installing. See [Troubleshooting and FAQ](Troubleshooting-and-FAQ#native-module-not-found-expo-go).
