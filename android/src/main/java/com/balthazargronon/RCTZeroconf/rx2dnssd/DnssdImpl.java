package com.balthazargronon.RCTZeroconf.rx2dnssd;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.util.Log;

import com.balthazargronon.RCTZeroconf.Zeroconf;
import com.balthazargronon.RCTZeroconf.ZeroconfModule;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.ReadableMapKeySetIterator;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeArray;
import com.facebook.react.bridge.WritableNativeMap;
import com.github.druk.rx2dnssd.BonjourService;
import com.github.druk.rx2dnssd.Rx2Dnssd;
import com.github.druk.rx2dnssd.Rx2DnssdEmbedded;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.List;

import javax.annotation.Nullable;

import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.disposables.Disposable;
import io.reactivex.schedulers.Schedulers;

public class DnssdImpl implements Zeroconf {
    private Rx2Dnssd rxDnssd;

    @Nullable
    private Disposable browseDisposable;

    private Map<String, BonjourService> mPublishedServices;
    private Map<String, Disposable> mRegisteredDisposables;

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
     * The daemonic version (Rx2DnssdBindable) is unreliable on Android as the
     * system daemon at /dev/socket/mdnsd doesn't exist on most devices.
     */
    private Rx2Dnssd createDnssd(Context context) {
        return new Rx2DnssdEmbedded(context);
    }

    @Override
    public void scan(String type, String protocol, String domain) {
        this.stop();

        if (multicastLock == null) {
            @SuppressLint("WifiManagerLeak") WifiManager wifi = (WifiManager) reactApplicationContext.getSystemService(Context.WIFI_SERVICE);
            multicastLock = wifi.createMulticastLock("multicastLock");
            multicastLock.setReferenceCounted(true);
            multicastLock.acquire();
        }

        String serviceType = getServiceType(type, protocol);
        Log.d("DnssdImpl", "Starting DNSSD scan for: " + serviceType);

        // Emit start event
        zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_START, null);

        browseDisposable = rxDnssd.browse(serviceType, "local.")
                .compose(rxDnssd.resolve())
                .compose(rxDnssd.queryRecords())
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe(bonjourService -> {
                    WritableMap service = serviceInfoToMap(bonjourService);
                    Log.d(getClass().getName(), service.toString());
                    zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_RESOLVE, service);
                }, throwable -> {
                    Log.e(getClass().getName(), "Error resolving service: ", throwable);
                    zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_ERROR, throwable.getMessage());
                });
    }

    private String getServiceType(String type, String protocol) {
        return String.format("_%s._%s", type, protocol);
    }

    private WritableMap serviceInfoToMap(BonjourService serviceInfo) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());
        final List<InetAddress> hostList = serviceInfo.getInetAddresses();
        final String fullServiceName;
        Log.d("TAG", serviceInfo.getServiceName());
        fullServiceName = serviceInfo.getServiceName();
        service.putString(ZeroconfModule.KEY_SERVICE_HOST, fullServiceName);

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
    public void stop() {
        if (browseDisposable != null) {
            browseDisposable.dispose();
            zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_STOP, null);
        }
        if (multicastLock != null) {
            multicastLock.release();
        }
        browseDisposable = null;
        multicastLock = null;
    }

    @Override
    public void unregisterService(String serviceName) {

        BonjourService bs = mPublishedServices.get(serviceName);
        if (bs != null) {
            zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_UNREGISTERED, serviceInfoToMap(bs));
            mPublishedServices.remove(serviceName);
        }

        Disposable registerDisposable = mRegisteredDisposables.get(serviceName);
        if (registerDisposable != null && !registerDisposable.isDisposed()) {
            registerDisposable.dispose();
            mRegisteredDisposables.remove(serviceName);
        }

    }

    @Override
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt) {
        BonjourService bs = new BonjourService.Builder(0, 0, name, getServiceType(type, protocol), null)
                .port(port)
                .dnsRecords(getTxtRecordMap(txt))
                .build();

        Disposable registerDisposable = rxDnssd.register(bs)
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe(bonjourService -> {
                    Log.i("TAG", "Register successfully " + bonjourService.toString());

                    mPublishedServices.put(bs.getServiceName(), bs);
                    zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_PUBLISHED, serviceInfoToMap(bonjourService));
                }, throwable -> {
                    Log.e("TAG", "error", throwable);
                });

        mRegisteredDisposables.put(name, registerDisposable);
    }

    private Map<String, String> getTxtRecordMap(ReadableMap txt) {
        Map<String, String> txtMap = new HashMap<>();
        ReadableMapKeySetIterator iterator = txt.keySetIterator();
        while (iterator.hasNextKey()) {
            String key = iterator.nextKey();
            txtMap.put(key, txt.getString(key));
        }
        return txtMap;
    }
}
