# Migrating to 0.17.1

From 0.17.0 to 0.17.1. Upgrading across several versions? Follow the [Migration Guide](Migration-Guide) from your version up.

| Change | Action needed? |
| --- | --- |
| Android and Windows: `resolveTimeout` applies, as on iOS. A service that doesn't resolve in time is retried once, then reported as an `error` with code `'TIMEOUT'` (scans were silent before) | Only if you treat every `error` as fatal: a `TIMEOUT` names one service in `serviceName`, the scan goes on |
| Android 13 and earlier: `resolveService()` rejects with `'TIMEOUT'` after `timeout`, it could wait indefinitely before | None |
