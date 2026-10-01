package com.balthazargronon.RCTZeroconf.dnssd;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.util.Log;

import com.balthazargronon.RCTZeroconf.Zeroconf;
import com.balthazargronon.RCTZeroconf.ZeroconfModule;
import com.balthazargronon.RCTZeroconf.ZeroconfOptions;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeArray;
import com.facebook.react.bridge.WritableNativeMap;
import com.github.druk.dnssd.BrowseListener;
import com.github.druk.dnssd.DNSSD;
import com.github.druk.dnssd.DNSSDEmbedded;
import com.github.druk.dnssd.DNSSDException;
import com.github.druk.dnssd.DNSSDRegistration;
import com.github.druk.dnssd.DNSSDService;
import com.github.druk.dnssd.NSClass;
import com.github.druk.dnssd.NSType;
import com.github.druk.dnssd.QueryListener;
import com.github.druk.dnssd.RegisterListener;
import com.github.druk.dnssd.ResolveListener;
import com.github.druk.dnssd.TXTRecord;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import javax.annotation.Nullable;

/**
 * Embedded mDNSResponder (dns_sd), the same browse, resolve and address queries as the iOS implementation.
 * DNSSD calls can block while the embedded daemon starts, so they and all the state below run on one
 * background thread. DNSSD delivers callbacks on the main thread, they are handed back to that thread.
 */
public class DnssdImpl implements Zeroconf {
    private static final String TAG = "DnssdImpl";

    private final ZeroconfModule zeroconfModule;
    private final ReactApplicationContext reactApplicationContext;
    private final DNSSD dnssd;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final android.os.Handler mainHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private WifiManager.MulticastLock multicastLock;

    // Running scans, keyed by the id of the JS instance that started them
    private final Map<String, Scan> scans = new HashMap<>();
    // Published services, keyed by the requested name
    private final Map<String, Publication> publications = new LinkedHashMap<>();

    private static class Scan {
        final String scanId;
        final boolean typesOnly;
        int ifIndex = DNSSD.ALL_INTERFACES;
        DNSSDService browse;
        // A service is reported once per network interface, count them so found/remove are emitted once
        final Map<String, Integer> foundInterfaces = new HashMap<>();
        final Map<String, Resolve> resolves = new HashMap<>();

        Scan(String scanId, boolean typesOnly) {
            this.scanId = scanId;
            this.typesOnly = typesOnly;
        }
    }

    private static class Resolve {
        final String name;
        // resolveService(): settled instead of emitting events
        @Nullable Promise promise;
        final List<DNSSDService> operations = new ArrayList<>();
        String fullName;
        String host;
        int port;
        Map<String, String> txt = new LinkedHashMap<>();
        final Set<String> addresses = new LinkedHashSet<>();

        Resolve(String name) {
            this.name = name;
        }

        void stop() {
            for (DNSSDService operation : operations) {
                operation.stop();
            }
            operations.clear();
        }
    }

    private static class Publication {
        final String requestedName;
        final int port;
        final Map<String, String> txt;
        DNSSDRegistration registration;
        @Nullable Promise promise;
        String name;
        String regType;
        String domain;
        boolean registered;

        Publication(String requestedName, int port, Map<String, String> txt, @Nullable Promise promise) {
            this.requestedName = requestedName;
            this.name = requestedName;
            this.port = port;
            this.txt = txt;
            this.promise = promise;
        }
    }

    public DnssdImpl(ZeroconfModule zeroconfModule, ReactApplicationContext reactApplicationContext) {
        this.zeroconfModule = zeroconfModule;
        this.reactApplicationContext = reactApplicationContext;
        // The embedded mDNSResponder works on every Android version, the system daemon is missing on most devices
        this.dnssd = new DNSSDEmbedded(reactApplicationContext);
    }

    // Scan

    @Override
    public void scan(final String scanId, final String type, final String protocol, final String domain, final ZeroconfOptions options) {
        executor.execute(() -> startScan(scanId, type, protocol, domain, options));
    }

    private void startScan(String scanId, String type, String protocol, String domain, ZeroconfOptions options) {
        stopScan(scanId);

        final Scan scan = new Scan(scanId, ZeroconfModule.isServiceTypesScan(type, protocol));
        final int ifIndex = interfaceIndex(options.networkInterface);
        if (ifIndex < 0) {
            zeroconfModule.sendError(ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_UNKNOWN_INTERFACE, "Unknown network interface " + options.networkInterface, null, scanId);
            return;
        }
        scan.ifIndex = ifIndex;
        // "_ipp._tcp,_printer" browses the _printer subtype
        String regType = String.format("_%s._%s", type, protocol);
        if (options.subtype != null) {
            regType = ZeroconfOptions.withSubtypes(regType, java.util.Collections.singletonList(options.subtype));
        }
        Log.d(TAG, "Starting DNSSD scan " + scanId + " for " + regType);
        acquireMulticastLock();
        try {
            scan.browse = dnssd.browse(0, ifIndex, regType, domainOrLocal(domain), new BrowseListener() {
                @Override
                public void serviceFound(DNSSDService browser, int flags, int ifIndex, String serviceName, String regType, String domain) {
                    executor.execute(() -> onServiceFound(scan, ifIndex, serviceName, regType, domain));
                }

                @Override
                public void serviceLost(DNSSDService browser, int flags, int ifIndex, String serviceName, String regType, String domain) {
                    executor.execute(() -> onServiceLost(scan, serviceName, regType));
                }

                @Override
                public void operationFailed(DNSSDService service, int errorCode) {
                    executor.execute(() -> {
                        if (scans.get(scan.scanId) != scan) {
                            return;
                        }
                        sendError(errorCode, "Browsing services failed: ", null, scan.scanId);
                        stopScan(scan.scanId);
                    });
                }
            });
        } catch (DNSSDException e) {
            releaseMulticastLock();
            sendError(e.getErrorCode(), "Browsing services failed: ", null, scanId);
            return;
        }
        scans.put(scanId, scan);
        sendScanEvent(ZeroconfModule.EVENT_START, new WritableNativeMap(), scanId);
    }

    private void onServiceFound(Scan scan, int ifIndex, String serviceName, String regType, String domain) {
        if (scans.get(scan.scanId) != scan) {
            return;
        }
        String name = scan.typesOnly ? ZeroconfModule.serviceTypeFromResult(serviceName, regType) : serviceName;
        Integer count = scan.foundInterfaces.get(name);
        scan.foundInterfaces.put(name, count == null ? 1 : count + 1);
        if (count != null) {
            return;
        }
        sendScanEvent(ZeroconfModule.EVENT_FOUND, nameToMap(name), scan.scanId);
        // Service types are not resolved
        if (!scan.typesOnly) {
            startResolve(scan, ifIndex, serviceName, regType, domain, null);
        }
    }

    private void onServiceLost(Scan scan, String serviceName, String regType) {
        if (scans.get(scan.scanId) != scan) {
            return;
        }
        String name = scan.typesOnly ? ZeroconfModule.serviceTypeFromResult(serviceName, regType) : serviceName;
        Integer count = scan.foundInterfaces.get(name);
        if (count == null) {
            return;
        }
        if (count > 1) {
            scan.foundInterfaces.put(name, count - 1);
            return;
        }
        scan.foundInterfaces.remove(name);
        Resolve resolve = scan.resolves.remove(name);
        if (resolve != null) {
            resolve.stop();
        }
        sendScanEvent(ZeroconfModule.EVENT_REMOVE, nameToMap(name), scan.scanId);
    }

    @Override
    public void stop(final String scanId) {
        executor.execute(() -> stopScan(scanId));
    }

    @Override
    public void stopAll() {
        executor.execute(() -> {
            for (String scanId : new ArrayList<>(scans.keySet())) {
                stopScan(scanId);
            }
        });
    }

    private void stopScan(String scanId) {
        Scan scan = scans.remove(scanId);
        if (scan == null) {
            return;
        }
        if (scan.browse != null) {
            scan.browse.stop();
        }
        for (Resolve resolve : scan.resolves.values()) {
            resolve.stop();
        }
        scan.resolves.clear();
        scan.foundInterfaces.clear();
        releaseMulticastLock();
        sendScanEvent(ZeroconfModule.EVENT_STOP, new WritableNativeMap(), scanId);
    }

    // Resolve: host, port and TXT record, then the host's IPv4 and IPv6 addresses

    private void startResolve(@Nullable final Scan scan, int ifIndex, final String serviceName, String regType, String domain, @Nullable Resolve existing) {
        final Resolve resolve = existing != null ? existing : new Resolve(serviceName);
        if (scan != null) {
            scan.resolves.put(serviceName, resolve);
        }
        try {
            resolve.operations.add(dnssd.resolve(0, ifIndex, serviceName, regType, domain, new ResolveListener() {
                @Override
                public void serviceResolved(DNSSDService resolver, int flags, int ifIndex, String fullName, String hostName, int port, Map<String, String> txtRecord) {
                    executor.execute(() -> {
                        if (!isCurrent(scan, resolve)) {
                            return;
                        }
                        resolve.fullName = fullName;
                        resolve.host = hostName;
                        resolve.port = port;
                        resolve.txt = txtRecord;
                        queryAddresses(scan, resolve, ifIndex);
                    });
                }

                @Override
                public void operationFailed(DNSSDService service, int errorCode) {
                    executor.execute(() -> onResolveFailed(scan, resolve, errorCode));
                }
            }));
        } catch (DNSSDException e) {
            onResolveFailed(scan, resolve, e.getErrorCode());
        }
    }

    private void queryAddresses(@Nullable final Scan scan, final Resolve resolve, int ifIndex) {
        QueryListener listener = new QueryListener() {
            @Override
            public void queryAnswered(DNSSDService query, int flags, int ifIndex, String fullName, int rrtype, int rrclass, byte[] rdata, int ttl) {
                executor.execute(() -> {
                    if (!isCurrent(scan, resolve)) {
                        return;
                    }
                    try {
                        resolve.addresses.add(InetAddress.getByAddress(rdata).getHostAddress());
                    } catch (UnknownHostException e) {
                        Log.w(TAG, "Invalid address record for " + resolve.host, e);
                        return;
                    }
                    if (scan == null) {
                        // resolveService() settles with the first address
                        endSingleResolve(resolve);
                        resolve.promise.resolve(resolveToMap(resolve));
                        return;
                    }
                    // Emitted for each address, the first one is usable right away
                    sendScanEvent(ZeroconfModule.EVENT_RESOLVE, resolveToMap(resolve), scan.scanId);
                });
            }

            @Override
            public void operationFailed(DNSSDService service, int errorCode) {
                executor.execute(() -> onResolveFailed(scan, resolve, errorCode));
            }
        };
        try {
            // Each query stops after its first answer, or silently after a timeout (no AAAA record for example)
            resolve.operations.add(dnssd.queryRecord(0, ifIndex, resolve.host, NSType.A, NSClass.IN, true, listener));
            resolve.operations.add(dnssd.queryRecord(0, ifIndex, resolve.host, NSType.AAAA, NSClass.IN, true, listener));
        } catch (DNSSDException e) {
            onResolveFailed(scan, resolve, e.getErrorCode());
        }
    }

    private boolean isCurrent(@Nullable Scan scan, Resolve resolve) {
        if (scan == null) {
            return singleResolves.contains(resolve);
        }
        return scans.get(scan.scanId) == scan && scan.resolves.get(resolve.name) == resolve;
    }

    private void onResolveFailed(@Nullable Scan scan, Resolve resolve, int errorCode) {
        if (!isCurrent(scan, resolve)) {
            return;
        }
        if (scan == null) {
            endSingleResolve(resolve);
            String message = "Resolving service " + resolve.name + " failed: " + ZeroconfModule.describeDnssdError(errorCode);
            ZeroconfModule.reject(resolve.promise, ZeroconfModule.ERROR_DOMAIN_DNSSD, errorCode, message, resolve.name);
            return;
        }
        scan.resolves.remove(resolve.name);
        resolve.stop();
        sendError(errorCode, "Resolving service " + resolve.name + " failed: ", resolve.name, scan.scanId);
    }

    // resolveService(): one service by name, without scanning

    private final Set<Resolve> singleResolves = new LinkedHashSet<>();

    @Override
    public void resolveService(final String name, final String type, final String protocol, final String domain, final ZeroconfOptions options, final Promise promise) {
        executor.execute(() -> {
            int ifIndex = interfaceIndex(options.networkInterface);
            if (ifIndex < 0) {
                ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_UNKNOWN_INTERFACE, "Unknown network interface " + options.networkInterface, null);
                return;
            }
            final Resolve resolve = new Resolve(name);
            resolve.promise = promise;
            singleResolves.add(resolve);
            acquireMulticastLock();
            startResolve(null, ifIndex, name, String.format("_%s._%s", type, protocol), domainOrLocal(domain), resolve);
            // DNSSD gives up silently after its own timeout, settle the promise before
            mainHandler.postDelayed(() -> executor.execute(() -> {
                if (!singleResolves.contains(resolve)) {
                    return;
                }
                endSingleResolve(resolve);
                ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_TIMEOUT, "Resolving service " + name + " failed: timed out", name);
            }), (long) (options.timeoutSeconds * 1000));
        });
    }

    private void endSingleResolve(Resolve resolve) {
        if (singleResolves.remove(resolve)) {
            resolve.stop();
            releaseMulticastLock();
        }
    }

    // Publish

    @Override
    public void registerService(final String type, final String protocol, final String domain, final String name, final int port, final ReadableArray txt, final ZeroconfOptions options, final Promise promise) {
        // Read the TXT record on the calling thread, the array belongs to the bridge call
        final Map<String, String> txtMap = getTxtRecordMap(txt);
        executor.execute(() -> startPublication(type, protocol, domain, name, port, txtMap, options, promise));
    }

    private void startPublication(String type, String protocol, String domain, String name, int port, Map<String, String> txt, ZeroconfOptions options, Promise promise) {
        int ifIndex = interfaceIndex(options.networkInterface);
        if (ifIndex < 0) {
            String message = "Unknown network interface " + options.networkInterface;
            zeroconfModule.sendError(ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_UNKNOWN_INTERFACE, message, name, null);
            ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_UNKNOWN_INTERFACE, message, name);
            return;
        }
        Publication previous = publications.remove(name);
        if (previous != null) {
            previous.registration.stop();
        }

        final Publication publication = new Publication(name, port, txt, promise);
        // "_ipp._tcp,_printer,_color" registers the subtypes too
        String regType = ZeroconfOptions.withSubtypes(String.format("_%s._%s", type, protocol), options.subtypes);
        try {
            publication.registration = dnssd.register(0, ifIndex, name, regType, null, null, port, toTXTRecord(txt), new RegisterListener() {
                @Override
                public void serviceRegistered(DNSSDRegistration registration, int flags, String serviceName, String regType, String domain) {
                    executor.execute(() -> onRegistered(publication, serviceName, regType, domain));
                }

                @Override
                public void operationFailed(DNSSDService service, int errorCode) {
                    executor.execute(() -> onRegistrationFailed(publication, errorCode));
                }
            });
        } catch (DNSSDException e) {
            String prefix = "Registering service " + name + " failed: ";
            sendError(e.getErrorCode(), prefix, name, null);
            ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_DNSSD, e.getErrorCode(), prefix + ZeroconfModule.describeDnssdError(e.getErrorCode()), name);
            return;
        }
        publications.put(name, publication);
    }

    private void onRegistered(Publication publication, String serviceName, String regType, String domain) {
        if (publications.get(publication.requestedName) != publication) {
            return;
        }
        publication.name = serviceName;
        publication.regType = regType;
        publication.domain = domain;
        if (publication.registered) {
            return;
        }
        publication.registered = true;
        Log.i(TAG, "Registered service " + serviceName);
        zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_PUBLISHED, publicationToMap(publication));
        if (publication.promise != null) {
            publication.promise.resolve(publicationToMap(publication));
            publication.promise = null;
        }
    }

    private void onRegistrationFailed(Publication publication, int errorCode) {
        if (publications.get(publication.requestedName) != publication) {
            return;
        }
        publications.remove(publication.requestedName);
        publication.registration.stop();
        String prefix = "Registering service " + publication.requestedName + " failed: ";
        sendError(errorCode, prefix, publication.requestedName, null);
        ZeroconfModule.reject(publication.promise, ZeroconfModule.ERROR_DOMAIN_DNSSD, errorCode, prefix + ZeroconfModule.describeDnssdError(errorCode), publication.requestedName);
        publication.promise = null;
    }

    @Override
    public void updateService(final String serviceName, final ReadableArray txt, final Promise promise) {
        final Map<String, String> txtMap = getTxtRecordMap(txt);
        executor.execute(() -> {
            Publication publication = findPublication(serviceName);
            if (publication == null || !publication.registered) {
                ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_NOT_PUBLISHED, "Service " + serviceName + " is not published", serviceName);
                return;
            }
            try {
                publication.registration.getTXTRecord().update(0, toTXTRecord(txtMap).getRawBytes(), 0);
            } catch (DNSSDException e) {
                String message = "Updating service " + serviceName + " failed: " + ZeroconfModule.describeDnssdError(e.getErrorCode());
                ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_DNSSD, e.getErrorCode(), message, serviceName);
                return;
            }
            publication.txt.clear();
            publication.txt.putAll(txtMap);
            promise.resolve(publicationToMap(publication));
        });
    }

    @Nullable
    private Publication findPublication(String serviceName) {
        // By the requested name, or the name it was published under when it was renamed
        Publication publication = publications.get(serviceName);
        if (publication == null) {
            for (Publication candidate : publications.values()) {
                if (candidate.name.equals(serviceName)) {
                    return candidate;
                }
            }
        }
        return publication;
    }

    @Override
    public void unregisterService(final String serviceName, @Nullable final Promise promise) {
        executor.execute(() -> stopPublication(serviceName, promise));
    }

    @Override
    public void unregisterAllServices() {
        executor.execute(() -> {
            for (String name : new ArrayList<>(publications.keySet())) {
                stopPublication(name, null);
            }
        });
    }

    private void stopPublication(String serviceName, @Nullable Promise promise) {
        Publication publication = findPublication(serviceName);
        if (publication == null) {
            ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_NOT_PUBLISHED, "Service " + serviceName + " is not published", serviceName);
            return;
        }
        publications.remove(publication.requestedName);
        // Stopping the registration unregisters synchronously
        publication.registration.stop();
        if (publication.promise != null) {
            ZeroconfModule.reject(publication.promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_NOT_PUBLISHED, "Service " + serviceName + " was unpublished before it was published", serviceName);
            publication.promise = null;
        }
        WritableMap service = publicationToMap(publication);
        if (publication.registered) {
            zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_UNREGISTERED, publicationToMap(publication));
        }
        if (promise != null) {
            promise.resolve(service);
        }
    }

    // Helpers

    // The index of a network interface by name, DNSSD.ALL_INTERFACES without one, -1 when it doesn't exist
    private static int interfaceIndex(@Nullable String networkInterface) {
        if (networkInterface == null) {
            return DNSSD.ALL_INTERFACES;
        }
        int index = DNSSD.getIfIndexForName(networkInterface);
        return index > 0 ? index : -1;
    }

    private static TXTRecord toTXTRecord(Map<String, String> txt) {
        TXTRecord txtRecord = new TXTRecord();
        for (Map.Entry<String, String> entry : txt.entrySet()) {
            txtRecord.set(entry.getKey(), entry.getValue());
        }
        return txtRecord;
    }

    private static String domainOrLocal(@Nullable String domain) {
        return domain == null || domain.isEmpty() ? "local." : domain;
    }

    private void sendScanEvent(String eventName, WritableMap body, String scanId) {
        body.putString(ZeroconfModule.KEY_SCAN_ID, scanId);
        zeroconfModule.sendEvent(reactApplicationContext, eventName, body);
    }

    private void sendError(int errorCode, String prefix, @Nullable String serviceName, @Nullable String scanId) {
        zeroconfModule.sendError(ZeroconfModule.ERROR_DOMAIN_DNSSD, errorCode, prefix + ZeroconfModule.describeDnssdError(errorCode), serviceName, scanId);
    }

    private void acquireMulticastLock() {
        if (multicastLock == null) {
            @SuppressLint("WifiManagerLeak") WifiManager wifi = (WifiManager) reactApplicationContext.getSystemService(Context.WIFI_SERVICE);
            multicastLock = wifi.createMulticastLock("multicastLock");
            multicastLock.setReferenceCounted(true);
        }
        multicastLock.acquire();
    }

    private void releaseMulticastLock() {
        if (multicastLock != null && multicastLock.isHeld()) {
            multicastLock.release();
        }
    }

    private static WritableMap nameToMap(String name) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, name);
        return service;
    }

    private static WritableMap resolveToMap(Resolve resolve) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, resolve.name);
        service.putString(ZeroconfModule.KEY_SERVICE_FULL_NAME, resolve.fullName);
        service.putString(ZeroconfModule.KEY_SERVICE_HOST, resolve.host);
        service.putInt(ZeroconfModule.KEY_SERVICE_PORT, resolve.port);
        WritableArray addresses = new WritableNativeArray();
        for (String address : resolve.addresses) {
            addresses.pushString(address);
        }
        service.putArray(ZeroconfModule.KEY_SERVICE_ADDRESSES, addresses);
        service.putMap(ZeroconfModule.KEY_SERVICE_TXT, txtToMap(resolve.txt));
        return service;
    }

    private static WritableMap publicationToMap(Publication publication) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, publication.name);
        service.putString(ZeroconfModule.KEY_SERVICE_FULL_NAME, publication.regType != null
                ? publication.name + "." + publication.regType + domainOrLocal(publication.domain)
                : publication.name);
        service.putString(ZeroconfModule.KEY_SERVICE_HOST, publication.name);
        service.putInt(ZeroconfModule.KEY_SERVICE_PORT, publication.port);
        service.putArray(ZeroconfModule.KEY_SERVICE_ADDRESSES, new WritableNativeArray());
        service.putMap(ZeroconfModule.KEY_SERVICE_TXT, txtToMap(publication.txt));
        return service;
    }

    private static WritableMap txtToMap(@Nullable Map<String, String> txt) {
        WritableMap map = new WritableNativeMap();
        if (txt != null) {
            for (Map.Entry<String, String> entry : txt.entrySet()) {
                map.putString(entry.getKey(), entry.getValue() != null ? entry.getValue() : "");
            }
        }
        return map;
    }

    private static Map<String, String> getTxtRecordMap(ReadableArray txt) {
        // LinkedHashMap keeps the TXT record order
        Map<String, String> txtMap = new LinkedHashMap<>();
        for (int i = 0; i < txt.size(); i++) {
            ReadableArray pair = txt.getArray(i);
            txtMap.put(pair.getString(0), pair.getString(1));
        }
        return txtMap;
    }
}
