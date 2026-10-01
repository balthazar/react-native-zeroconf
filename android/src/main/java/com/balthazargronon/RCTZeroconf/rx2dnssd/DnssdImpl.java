package com.balthazargronon.RCTZeroconf.rx2dnssd;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.util.Log;

import com.balthazargronon.RCTZeroconf.Zeroconf;
import com.balthazargronon.RCTZeroconf.ZeroconfModule;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeArray;
import com.facebook.react.bridge.WritableNativeMap;
import com.github.druk.dnssd.DNSSDException;
import com.github.druk.rx2dnssd.BonjourService;
import com.github.druk.rx2dnssd.Rx2Dnssd;
import com.github.druk.rx2dnssd.Rx2DnssdEmbedded;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.List;

import javax.annotation.Nullable;

import io.reactivex.Flowable;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.disposables.Disposable;
import io.reactivex.schedulers.Schedulers;

public class DnssdImpl implements Zeroconf {
    private static final String TAG = "DnssdImpl";

    private Rx2Dnssd rxDnssd;


    private Map<String, BonjourService> mPublishedServices;
    private Map<String, Disposable> mRegisteredDisposables;

    // Running browses, keyed by the id of the JS instance that started them
    private final Map<String, Disposable> mBrowses = new ConcurrentHashMap<>();

    private ZeroconfModule zeroconfModule;

    private ReactApplicationContext reactApplicationContext;
    private WifiManager.MulticastLock multicastLock;

    public DnssdImpl(ZeroconfModule zeroconfModule, ReactApplicationContext reactApplicationContext) {
        this.zeroconfModule = zeroconfModule;
        this.reactApplicationContext = reactApplicationContext;
        mPublishedServices = new HashMap<String, BonjourService>();
        mRegisteredDisposables = new HashMap<String, Disposable>();
        rxDnssd = createDnssd(reactApplicationContext);
    }

    /**
     * Creates the Rx2Dnssd implementation.
     * Always uses embedded mDNSResponder since it works across all Android versions.
     * The daemonic version (not bundled) is unreliable on Android as the
     * system daemon at /dev/socket/mdnsd doesn't exist on most devices.
     */
    private Rx2Dnssd createDnssd(Context context) {
        return new Rx2DnssdEmbedded(context);
    }

    @Override
    public void scan(final String scanId, String type, String protocol, String domain) {
        this.stop(scanId);
        acquireMulticastLock();

        String serviceType = getServiceType(type, protocol);
        Log.d(TAG, "Starting DNSSD scan " + scanId + " for: " + serviceType);

        sendScanEvent(ZeroconfModule.EVENT_START, new WritableNativeMap(), scanId);

        // A service is reported once per network interface, count them so found/remove are emitted once
        final Map<String, Integer> foundInterfaces = new HashMap<>();
        final boolean typesOnly = ZeroconfModule.isServiceTypesScan(type, protocol);
        Disposable browse = rxDnssd.browse(serviceType, "local.")
                .doOnNext(bonjourService -> {
                    String name = typesOnly
                            ? ZeroconfModule.serviceTypeFromResult(bonjourService.getServiceName(), bonjourService.getRegType())
                            : bonjourService.getServiceName();
                    Integer count = foundInterfaces.get(name);
                    int interfaces = count == null ? 0 : count;
                    if (!bonjourService.isLost()) {
                        foundInterfaces.put(name, interfaces + 1);
                        if (interfaces == 0) {
                            sendScanEvent(ZeroconfModule.EVENT_FOUND, nameToMap(name), scanId);
                        }
                    } else if (interfaces <= 1) {
                        foundInterfaces.remove(name);
                        sendScanEvent(ZeroconfModule.EVENT_REMOVE, nameToMap(name), scanId);
                    } else {
                        foundInterfaces.put(name, interfaces - 1);
                    }
                })
                // Service types are not resolved
                .filter(bonjourService -> !typesOnly && !bonjourService.isLost())
                // Resolve each service independently so a single failure doesn't end the whole browse
                .flatMap(bonjourService -> Flowable.just(bonjourService)
                                .compose(rxDnssd.resolve())
                                .compose(rxDnssd.queryRecords())
                                .onErrorResumeNext((Throwable throwable) -> {
                                    Log.e(TAG, "Error resolving service: ", throwable);
                                    sendError(throwable, "Resolving service " + bonjourService.getServiceName() + " failed: ", bonjourService.getServiceName(), scanId);
                                    return Flowable.empty();
                                }))
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe(bonjourService -> {
                    // Lost services were filtered out above, a lost service could still come back from resolve
                    if (bonjourService.isLost()) {
                        return;
                    }
                    sendScanEvent(ZeroconfModule.EVENT_RESOLVE, serviceInfoToMap(bonjourService), scanId);
                }, throwable -> {
                    Log.e(TAG, "Error browsing services: ", throwable);
                    sendError(throwable, "Browsing services failed: ", null, scanId);
                    if (mBrowses.remove(scanId) != null) {
                        releaseMulticastLock();
                    }
                    sendScanEvent(ZeroconfModule.EVENT_STOP, new WritableNativeMap(), scanId);
                });
        mBrowses.put(scanId, browse);
    }

    private void sendScanEvent(String eventName, WritableMap body, String scanId) {
        body.putString(ZeroconfModule.KEY_SCAN_ID, scanId);
        zeroconfModule.sendEvent(reactApplicationContext, eventName, body);
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

    /**
     * DNSSD errors carry their DNSServiceErrorType code, anything else is reported as an exception
     */
    private void sendError(Throwable throwable, String prefix, @Nullable String serviceName, @Nullable String scanId) {
        if (throwable instanceof DNSSDException) {
            DNSSDException dnssdException = (DNSSDException) throwable;
            zeroconfModule.sendError(ZeroconfModule.ERROR_DOMAIN_DNSSD, dnssdException.getErrorCode(), prefix + throwable.getMessage(), serviceName, scanId);
        } else {
            zeroconfModule.sendError(ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_EXCEPTION, prefix + throwable.getMessage(), serviceName, scanId);
        }
    }

    private void rejectWith(@Nullable Promise promise, Throwable throwable, String prefix, @Nullable String serviceName) {
        if (throwable instanceof DNSSDException) {
            ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_DNSSD, ((DNSSDException) throwable).getErrorCode(), prefix + throwable.getMessage(), serviceName);
        } else {
            ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_EXCEPTION, prefix + throwable.getMessage(), serviceName);
        }
    }

    private WritableMap nameToMap(String name) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, name);
        return service;
    }

    private String getServiceType(String type, String protocol) {
        return String.format("_%s._%s", type, protocol);
    }

    private WritableMap serviceInfoToMap(BonjourService serviceInfo) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());
        final List<InetAddress> hostList = serviceInfo.getInetAddresses();
        final String hostname = serviceInfo.getHostname();
        final String fullServiceName = hostname != null
                ? hostname + serviceInfo.getRegType()
                : serviceInfo.getServiceName();
        service.putString(ZeroconfModule.KEY_SERVICE_HOST, hostname != null ? hostname : serviceInfo.getServiceName());

        WritableArray addresses = new WritableNativeArray();
        for (InetAddress host : hostList) {
            addresses.pushString(host.getHostAddress());
        }

        service.putArray(ZeroconfModule.KEY_SERVICE_ADDRESSES, addresses);
        service.putString(ZeroconfModule.KEY_SERVICE_FULL_NAME, fullServiceName);
        service.putInt(ZeroconfModule.KEY_SERVICE_PORT, serviceInfo.getPort());

        WritableMap txtRecords = new WritableNativeMap();

        Map<String, String> attributes = serviceInfo.getTxtRecords();
        for (String key : attributes.keySet()) {
            String recordValue = attributes.get(key);
            txtRecords.putString(String.format(Locale.getDefault(), "%s", key), String.format(Locale.getDefault(), "%s", recordValue != null ? recordValue : ""));
        }

        service.putMap(ZeroconfModule.KEY_SERVICE_TXT, txtRecords);

        return service;
    }

    @Override
    public void stop(String scanId) {
        Disposable browse = mBrowses.remove(scanId);
        if (browse == null) {
            return;
        }
        browse.dispose();
        releaseMulticastLock();
        sendScanEvent(ZeroconfModule.EVENT_STOP, new WritableNativeMap(), scanId);
    }

    @Override
    public void stopAll() {
        for (String scanId : new ArrayList<>(mBrowses.keySet())) {
            stop(scanId);
        }
    }

    @Override
    public void unregisterService(String serviceName, @Nullable Promise promise) {

        BonjourService bs = mPublishedServices.get(serviceName);
        Disposable registerDisposable = mRegisteredDisposables.get(serviceName);
        if (bs == null && registerDisposable == null) {
            ZeroconfModule.reject(promise, ZeroconfModule.ERROR_DOMAIN_LIBRARY, ZeroconfModule.ERROR_CODE_NOT_PUBLISHED, "Service " + serviceName + " is not published", serviceName);
            return;
        }

        if (bs != null) {
            zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_UNREGISTERED, serviceInfoToMap(bs));
            mPublishedServices.remove(serviceName);
        }

        if (registerDisposable != null && !registerDisposable.isDisposed()) {
            registerDisposable.dispose();
        }
        mRegisteredDisposables.remove(serviceName);

        // Disposing unregisters synchronously
        if (promise != null) {
            promise.resolve(bs != null ? serviceInfoToMap(bs) : null);
        }
    }

    @Override
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableArray txt, Promise promise) {
        BonjourService bs = new BonjourService.Builder(0, 0, name, getServiceType(type, protocol), null)
                .port(port)
                .dnsRecords(getTxtRecordMap(txt))
                .build();

        // Resolved once, the registration stream can emit again later
        final Promise[] registerPromise = { promise };
        Disposable registerDisposable = rxDnssd.register(bs)
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe(bonjourService -> {
                    Log.i(TAG, "Registered service " + bonjourService.toString());

                    mPublishedServices.put(bs.getServiceName(), bs);
                    zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_PUBLISHED, serviceInfoToMap(bonjourService));
                    if (registerPromise[0] != null) {
                        registerPromise[0].resolve(serviceInfoToMap(bonjourService));
                        registerPromise[0] = null;
                    }
                }, throwable -> {
                    Log.e(TAG, "Error registering service: ", throwable);
                    sendError(throwable, "Registering service " + name + " failed: ", name, null);
                    rejectWith(registerPromise[0], throwable, "Registering service " + name + " failed: ", name);
                    registerPromise[0] = null;
                    mRegisteredDisposables.remove(name);
                });

        mRegisteredDisposables.put(name, registerDisposable);
    }

    @Override
    public void unregisterAllServices() {
        for (String serviceName : new ArrayList<>(mRegisteredDisposables.keySet())) {
            unregisterService(serviceName, null);
        }
    }

    private Map<String, String> getTxtRecordMap(ReadableArray txt) {
        // LinkedHashMap keeps the TXT record order
        Map<String, String> txtMap = new LinkedHashMap<>();
        for (int i = 0; i < txt.size(); i++) {
            ReadableArray pair = txt.getArray(i);
            txtMap.put(pair.getString(0), pair.getString(1));
        }
        return txtMap;
    }
}
