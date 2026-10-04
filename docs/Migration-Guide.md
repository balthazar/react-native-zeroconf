# Migration Guide

One page per minor version, with its patch releases. Upgrading across several versions? Read the pages from your version up. Rebuild the app after any upgrade (`pod install` on iOS), the native code changes between versions.

| Upgrading to | From | What changes |
| --- | --- | --- |
| [1.0](Migrating-to-1.0) (release candidate) | 0.17 | One C++ native module on every platform: React Native 0.82 and the New Architecture, the positional forms of `scan()` and `publishService()` removed |
| [0.17](Migrating-to-0.17) | 0.16 | Windows, service types, subtypes, `updateService()` and `resolveService()`; no breaking change. Its patch releases are on the same page |
| [0.16](Migrating-to-0.16) | 0.15 | Structured errors, promises from publishing, options objects |
| [0.15](Migrating-to-0.15) | 0.14 and earlier | Android `DNSSD` removals and host names, build requirements |
