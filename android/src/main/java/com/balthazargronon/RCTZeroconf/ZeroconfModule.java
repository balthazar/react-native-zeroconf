package com.balthazargronon.RCTZeroconf;


import android.util.Log;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
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

    // Where an error code comes from
    public static final String ERROR_DOMAIN_NSD = "NsdManager";
    public static final String ERROR_DOMAIN_DNSSD = "DNSSD";
    public static final String ERROR_DOMAIN_LIBRARY = "RNZeroconf";
    public static final String ERROR_CODE_EXCEPTION = "EXCEPTION";

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
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception during scan: " + e.getMessage(), null);
        }
    }

    @ReactMethod
    public void stop(String implType) {
        try {
            getZeroconfImpl(implType).stop();
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception during stop: " + e.getMessage(), null);
        }
    }

    private Zeroconf getZeroconfImpl(String implType) {
        return zeroConfFactory.getZeroconf(implType);
    }

    @ReactMethod
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableArray txt, String implType) {
        try {
            getZeroconfImpl(implType).registerService(type, protocol, domain, name, port, txt);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception while registering service: " + e.getMessage(), name);
        }
    }

    @ReactMethod
    public void unregisterService(String serviceName, String implType) {
        try {
            getZeroconfImpl(implType).unregisterService(serviceName);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception while unregistering service: " + e.getMessage(), serviceName);
        }
    }

    public void sendEvent(ReactContext reactContext,
                          String eventName,
                          @Nullable Object params) {
        try {
            reactContext
                    .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                    .emit(eventName, params);
        } catch (Throwable e) {
            // The JS side may already be gone, e.g. callbacks arriving during teardown
            Log.w(getClass().getName(), "Could not send " + eventName + ": " + e.getMessage());
        }
    }

    /**
     * Emits an error event as { message, code, domain, serviceName }
     *
     * @param code an Integer for platform error codes, a String for the library's own
     */
    public void sendError(String domain, Object code, String message, @Nullable String serviceName) {
        WritableMap error = new WritableNativeMap();
        error.putString("message", message);
        error.putString("domain", domain);
        if (code instanceof Integer) {
            error.putInt("code", (Integer) code);
        } else {
            error.putString("code", String.valueOf(code));
        }
        if (serviceName != null) {
            error.putString("serviceName", serviceName);
        }
        sendEvent(getReactApplicationContext(), EVENT_ERROR, error);
    }

    /**
     * Readable description of an NsdManager failure code
     */
    public static String describeNsdError(int errorCode) {
        switch (errorCode) {
            case 0: return "internal error"; // FAILURE_INTERNAL_ERROR
            case 3: return "operation already active"; // FAILURE_ALREADY_ACTIVE
            case 4: return "maximum number of requests reached"; // FAILURE_MAX_LIMIT
            case 5: return "operation not running"; // FAILURE_OPERATION_NOT_RUNNING (API 34)
            case 6: return "bad parameters"; // FAILURE_BAD_PARAMETERS (API 34)
            default: return "error " + errorCode;
        }
    }

    // Called on teardown by current React Native versions.
    // No @Override so it still compiles against versions that don't declare it.
    public void invalidate() {
        teardown();
    }

    // Called on teardown by older React Native versions, deprecated in favor of invalidate().
    // No @Override so it still compiles once it is removed.
    public void onCatalystInstanceDestroy() {
        teardown();
    }

    private void teardown() {
        try {
            for (Zeroconf impl : zeroConfFactory.getCreatedImpls()) {
                impl.stop();
                impl.unregisterAllServices();
            }
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
        }
    }
}
