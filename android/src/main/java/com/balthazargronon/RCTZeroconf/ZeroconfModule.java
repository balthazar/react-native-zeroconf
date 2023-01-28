package com.balthazargronon.RCTZeroconf;


import android.util.Log;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;

import javax.annotation.Nullable;


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

    private ZeroConfImplFactory zeroConfFactory;

    public ZeroconfModule(ReactApplicationContext reactContext) {
        super(reactContext);
        zeroConfFactory = new ZeroConfImplFactory(this, getReactApplicationContext());
    }

    @Override
    public String getName() {
        return "RNZeroconf";
    }

    @ReactMethod
    public void scan(String type, String protocol, String domain, String implType) {
        try {
            getZeroconfImpl(implType).scan(type, protocol, domain);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, "Exception During Scan: " + e.getMessage());
        }
    }

    @ReactMethod
    public void stop(String implType) {
        try {
            getZeroconfImpl(implType).stop();
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, "Exception During Stop: " + e.getMessage());
        }
    }

    private Zeroconf getZeroconfImpl(String implType) {
        return zeroConfFactory.getZeroconf(implType);
    }

    @ReactMethod
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt, String implType) {
        try {
            getZeroconfImpl(implType).registerService(type, protocol, domain, name, port, txt);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, "Exception During Register Service: " + e.getMessage());
        }
    }

    @ReactMethod
    public void unregisterService(String serviceName, String implType) {
        try {
            getZeroconfImpl(implType).unregisterService(serviceName);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, "Exception During Unregister Service: " + e.getMessage());
        }
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
        try {
            stop(ZeroConfImplFactory.NSD_IMPL);
            stop(ZeroConfImplFactory.DNSSD_IMPL);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, "Exception During Catalyst Destroy: " + e.getMessage());
        }
    }
}
