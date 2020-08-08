package com.balthazargronon.RCTZeroconf;


import com.balthazargronon.RCTZeroconf.nsd.NsdServiceImpl;
import com.balthazargronon.RCTZeroconf.rx2dnssd.DnssdImpl;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;

import org.apache.commons.lang3.StringUtils;

import javax.annotation.Nullable;
import java.util.HashMap;
import java.util.Map;


/**
 * Created by Jeremy White on 8/1/2016.
 * Copyright © 2016 Balthazar Gronon MIT
 */
public class ZeroconfModule extends ReactContextBaseJavaModule {

    public static final String EVENT_START = "RNZeroconfStart";
    public static final String EVENT_STOP = "RNZeroconfStop";
    public static final String EVENT_ERROR = "RNZeroconfError";
    public static final String EVENT_FOUND = "RNZeroconfFound";
    public static final String EVENT_REMOVE = "RNZeroconfRemove";
    public static final String EVENT_RESOLVE = "RNZeroconfResolved";

    public static final String EVENT_PUBLISHED = "RNZeroconfServiceRegistered";
    public static final String EVENT_UNREGISTERED = "RNZeroconfServiceUnregistered";

    public static final String KEY_SERVICE_NAME = "name";
    public static final String KEY_SERVICE_FULL_NAME = "fullName";
    public static final String KEY_SERVICE_HOST = "host";
    public static final String KEY_SERVICE_PORT = "port";
    public static final String KEY_SERVICE_ADDRESSES = "addresses";
    public static final String KEY_SERVICE_TXT = "txt";

    private static final String NSD_IMPL = "NSD";

    private static final String DNSSD_IMPL = "DNSSD";

    private Map<String, Zeroconf> zeroconfMap;

    // TBD: I do not have sufficient expertise on IOS and hence didn't add this as a new parameter
    // I have added it as a constant. Will leave this to @balthazar
    private String IMPL_TYPE = DNSSD_IMPL;



    public ZeroconfModule(ReactApplicationContext reactContext) {
        super(reactContext);
        zeroconfMap = new HashMap<>();
        zeroconfMap.put(NSD_IMPL, new NsdServiceImpl(this, getReactApplicationContext()));
        zeroconfMap.put(DNSSD_IMPL, new DnssdImpl(this, getReactApplicationContext()));
    }

    @Override
    public String getName() {
        return "RNZeroconf";
    }

    @ReactMethod
    public void scan(String type, String protocol, String domain, String implType) {
        getZeroconfImpl(implType).scan(type, protocol, domain);
    }

    @ReactMethod
    public void stop(String implType) {
        getZeroconfImpl(implType).stop();
    }

    private Zeroconf getZeroconfImpl(String implType) {
        if (StringUtils.isBlank(implType)) implType = NSD_IMPL;

        Zeroconf zeroconf = zeroconfMap.get(implType);
        if (zeroconf == null)
            throw new IllegalArgumentException(String.format("%s implType is not supported. Only %s and %s are supported", implType, NSD_IMPL, DNSSD_IMPL));
        return zeroconf;
    }

    @ReactMethod
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt, String implType) {
        getZeroconfImpl(implType).registerService(type, protocol, domain, name, port, txt);
    }

    @ReactMethod
    public void unregisterService(String serviceName, String implType) {
        getZeroconfImpl(implType).unregisterService(serviceName);
    }

    public void sendEvent(ReactContext reactContext,
                          String eventName,
                          @Nullable Object params) {
        reactContext
                .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                .emit(eventName, params);
    }

    @Override
    public void onCatalystInstanceDestroy() {
        super.onCatalystInstanceDestroy();
        stop(NSD_IMPL);
        stop(DNSSD_IMPL);
    }
}
