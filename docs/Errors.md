# Errors

Errors from the `error` event and from promise rejections are `Error` objects with extra fields:

| Field | Type | Description |
| --- | --- | --- |
| `message` | `string` | Readable description |
| `code` | `number \| string` | Numeric platform code, or a string code from the library |
| `domain` | `string` | Where `code` comes from (table below) |
| `serviceName` | `string?` | The service concerned, when there is one |

## Domains

| `domain` | Platform | `code` |
| --- | --- | --- |
| `'DNSSD'` | iOS | `DNSServiceErrorType` from Apple's dns_sd API (negative number) |
| `'DNSSD'` | Android (`implType: 'DNSSD'`) | `DNSServiceErrorType` from the embedded mDNSResponder |
| `'NsdManager'` | Android (`implType: 'NSD'`) | `NsdManager` failure code |
| `'Windows'` | Windows | Win32 or `DNS_STATUS` code from the Windows DNS-SD functions, `message` has the system description |
| `'RNZeroconf'` | all | String code from the library |

## Common codes

| Domain | Code | Meaning |
| --- | --- | --- |
| `DNSSD` | `-65555` | `kDNSServiceErr_NoAuth`. iOS: the service type is missing from `NSBonjourServices` ("not authorized, add the service type to NSBonjourServices in Info.plist"). Also returned by `scanServiceTypes()` on iOS without the multicast entitlement, see [Listing service types](Scanning#listing-service-types) |
| `DNSSD` | `-65570` | `kDNSServiceErr_PolicyDenied`. iOS: Local Network access denied |
| `DNSSD` | `-65548` | `kDNSServiceErr_NameConflict`, name already in use |
| `DNSSD` | `-65540` | `kDNSServiceErr_BadParam` |
| `DNSSD` | `-65554` | `kDNSServiceErr_NoSuchRecord` |
| `DNSSD` | `-65563` | `kDNSServiceErr_ServiceNotRunning`, mDNS daemon not running |
| `DNSSD` | `-65568` | `kDNSServiceErr_Timeout` |
| `NsdManager` | `0` | `FAILURE_INTERNAL_ERROR` |
| `NsdManager` | `3` | `FAILURE_ALREADY_ACTIVE` |
| `NsdManager` | `4` | `FAILURE_MAX_LIMIT`, too many requests |
| `NsdManager` | `5` | `FAILURE_OPERATION_NOT_RUNNING` (API 34+) |
| `NsdManager` | `6` | `FAILURE_BAD_PARAMETERS` (API 34+) |
| `NsdManager` | `7` | `FAILURE_PERMISSION_DENIED` (API 37+): the app needs `ACCESS_LOCAL_NETWORK`, see [Permissions and Setup](Permissions-and-Setup#android-17-local-network-permission) |
| `RNZeroconf` | `'EXCEPTION'` | Unexpected native exception, see `message` |
| `RNZeroconf` | `'NOT_PUBLISHED'` | `unpublishService()` with a name that is not published |
| `RNZeroconf` | `'TIMEOUT'` | iOS: a service could not be resolved within `resolveTimeout` (after one retry). All platforms: `resolveService()` got no answer within `timeout` |
| `RNZeroconf` | `'UNKNOWN_INTERFACE'` | The `networkInterface` passed doesn't exist |
| `RNZeroconf` | `'UNSUPPORTED'` | Android `NSD` before Android 13: `networkInterface` is not supported, use `DNSSD`. Windows: publishing with `subtypes` |
| `RNZeroconf` | `'TXT_ENTRY_TOO_LONG'` | iOS: a TXT entry over 255 bytes was left out |
| `RNZeroconf` | `'MISSING_BONJOUR_SERVICES'` | iOS: `checkLocalNetworkAccess()` has no type to test with |

```javascript
zeroconf.on('error', error => {
  if (error.domain === 'DNSSD' && error.code === -65555) {
    // iOS: add the service type to NSBonjourServices
  } else if (error.domain === 'DNSSD' && error.code === -65570) {
    // iOS: Local Network access denied, send the user to Settings
  }
})
```

---

See [Error Handling](Error-Handling) for how to react to them.
