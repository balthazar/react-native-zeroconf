# Error Handling

How errors are reported, and how to tell them apart.

Errors arrive through the `error` event (scan and resolve failures) and as promise rejections (`publishService`, `unpublishService`, `checkLocalNetworkAccess`). Both have the same shape:

```javascript
zeroconf.on('error', error => {
  // error.message, error.code, error.domain, error.serviceName
  if (error.domain === 'DNSSD' && error.code === -65555) {
    // iOS: the service type is missing from NSBonjourServices
  } else if (error.domain === 'DNSSD' && error.code === -65570) {
    // iOS: Local Network access denied
  } else if (error.domain === 'RNZeroconf' && error.code === 'TIMEOUT') {
    // iOS: could not resolve error.serviceName in time
  }
})
```

> Add an `error` listener. Native errors are only emitted to JavaScript when at least one `error` listener is registered.

The full list of domains and codes is in the [API Reference](Errors).

---

See the [API Reference](API-Reference) for every option, event and error code.
