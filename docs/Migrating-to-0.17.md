# Migrating to 0.17

From 0.16 to 0.17. Upgrading across several versions? Follow the [Migration Guide](Migration-Guide) from your version up.

0.17 has no breaking change in the JavaScript API. Rebuild the app after upgrading (`pod install` on iOS), the native code changed.

| Change | Action needed? |
| --- | --- |
| Android: `checkLocalNetworkAccess()` resolves `'granted'` or `'denied'` instead of `'unknown'`, and can show the Android 17 permission prompt | Pass `request: false` to only check |
| iOS: `resolved` fires again when a service's addresses or TXT record change, as on Android 14+ with `NSD` | Update your list by `service.name` rather than appending |
| Android `DNSSD`: resolve and publish errors carry their `DNSSD` code (they were `RNZeroconf` `'EXCEPTION'`), published services report their port | Only if you matched `'EXCEPTION'` |
| Android `DNSSD` no longer uses RxJava | None |
| New: [Windows](Installation-Windows), [`scanServiceTypes()` / `useServiceTypes()`](Scanning#listing-service-types), [subtypes](Scanning#subtypes), [`networkInterface`](Scanning#choosing-a-network-interface), [`updateService()`](Publishing#updating-the-txt-record), [`resolveService()`](Scanning#resolving-a-single-service), the [Android 17 permission](Permissions-and-Setup#android-17-local-network-permission) | Optional |
