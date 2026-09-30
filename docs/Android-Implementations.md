# Android Implementations

On Android you choose the backend per call with `implType`:

| | `NSD` (default) | `DNSSD` |
| --- | --- | --- |
| Engine | Android's `NsdManager` | Apple mDNSResponder embedded in the library (from Discord's RxDNSSD fork) |
| Depends on | The system NSD service, varies by Android version and manufacturer | Only the library's own native code |
| `host` field | mDNS host name on Android 16+, usually the IP before | mDNS host name |
| Addresses | All on Android 14+, a single one before | All |
| TXT order when publishing | Decided by the system | Preserved |
| Resolving | Android 14+: services followed with `registerServiceInfoCallback` (parallel, updates re-emitted). Older: one resolve at a time | Per service |
| Error domain | `'NsdManager'` | `'DNSSD'` |

```javascript
import Zeroconf, { ImplType } from 'react-native-zeroconf'

zeroconf.scan({ type: 'http', implType: ImplType.DNSSD })
zeroconf.publishService({ type: 'http', protocol: 'tcp', name: 'MyServer', port: 8080, implType: ImplType.DNSSD })
```

On iOS, macOS and tvOS `implType` is ignored.

## Which one should I use?

Start with **`NSD`** if you target recent Android versions and it finds your devices.

Switch to **`DNSSD`** when:

- `NSD` finds nothing, or stops finding services after a few scans (common on some OEM builds),
- you need the `host` name on Android versions before 16,
- you publish TXT records whose order matters,
- you discover printers (`pdl-datastream`, `ipp`) and `NSD` misses them.

> Native code in the library is built with 16 KB page alignment, which Google Play requires for apps targeting Android 15+.

## Rules of thumb

- **One scan per instance.** Calling `scan()` with a different `implType` on the same instance moves that instance's scan to the other implementation. Separate instances can scan at the same time, and `NSD` and `DNSSD` scans can run concurrently.
- `stop()` without arguments stops the implementation the instance's last `scan()` used.
- `unpublishService(name)` without `implType` uses the implementation the service was published with.
- After `stop()`, give `DNSSD` a short delay (around 500 ms) before scanning again.

## Known NSD reliability issues

These are platform issues that affect every mDNS library using `NsdManager`:

- discovery silently stops after a while, after screen lock or when backgrounded,
- a service is found but its resolve fails,
- multicast is throttled by battery optimizations on some manufacturers (Samsung, Xiaomi, Huawei...),
- network changes (Wi-Fi reconnect, band switch, mesh handoff, VPN) break discovery.

Workarounds: use `DNSSD`, stop the scan when the app goes to the background and scan again when it becomes active, and retry a scan that returned nothing. See [Troubleshooting and FAQ](Troubleshooting-and-FAQ#android-discovery-stops-or-is-unreliable).

## References

- [NsdManager](https://developer.android.com/reference/android/net/nsd/NsdManager)
- [RxDNSSD (Discord fork)](https://github.com/discord/RxDNSSD)
