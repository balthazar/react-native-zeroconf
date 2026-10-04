# Installation on Android

After [installing the package](Installation) there is nothing else to do: autolinking builds the module's C++ into the app with the NDK the New Architecture already uses, and the required permissions are merged from the library manifest. Rebuild the app with `yarn android`.

- Apps targeting Android 17 (API 37) also need the local network permission, see [Permissions and Setup](Permissions-and-Setup#android-17-local-network-permission).
- Two implementations are available, Android's `NsdManager` (default) and an embedded mDNSResponder, see [Android Implementations](Android-Implementations).
- Testing on the emulator? See [Android Emulator](Android-Emulator).
