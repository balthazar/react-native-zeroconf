package com.balthazargronon.RCTZeroconf;


import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.os.Build;
import android.util.Log;

import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
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
    // Id of the JS instance whose scan an event belongs to
    public static final String KEY_SCAN_ID = "scanId";

    // Where an error code comes from
    public static final String ERROR_DOMAIN_NSD = "NsdManager";
    public static final String ERROR_DOMAIN_DNSSD = "DNSSD";
    public static final String ERROR_DOMAIN_LIBRARY = "RNZeroconf";
    public static final String ERROR_CODE_EXCEPTION = "EXCEPTION";
    public static final String ERROR_CODE_TIMEOUT = "TIMEOUT";
    public static final String ERROR_CODE_UNKNOWN_INTERFACE = "UNKNOWN_INTERFACE";
    public static final String ERROR_CODE_UNSUPPORTED = "UNSUPPORTED";
    // Manifest.permission.ACCESS_LOCAL_NETWORK, API 37
    public static final String PERMISSION_ACCESS_LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK";
    public static final String ERROR_CODE_NOT_PUBLISHED = "NOT_PUBLISHED";

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
    public void scan(String scanId, String type, String protocol, String domain, String implType, @Nullable ReadableMap options) {
        try {
            getZeroconfImpl(implType).scan(scanId, type, protocol, domain, ZeroconfOptions.from(options));
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception during scan: " + e.getMessage(), null, scanId);
        }
    }

    /**
     * Stops the scan with this id, or every scan of the implementation without one
     */
    @ReactMethod
    public void stop(@Nullable String scanId, String implType) {
        try {
            Zeroconf impl = getZeroconfImpl(implType);
            if (scanId == null || scanId.isEmpty()) {
                impl.stopAll();
            } else {
                impl.stop(scanId);
            }
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception during stop: " + e.getMessage(), null, scanId);
        }
    }

    private Zeroconf getZeroconfImpl(String implType) {
        return zeroConfFactory.getZeroconf(implType);
    }

    @ReactMethod
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableArray txt, String implType, @Nullable ReadableMap options, Promise promise) {
        try {
            getZeroconfImpl(implType).registerService(type, protocol, domain, name, port, txt, ZeroconfOptions.from(options), promise);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            String message = "Exception while registering service: " + e.getMessage();
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, name);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, name);
        }
    }

    @ReactMethod
    public void updateService(String serviceName, ReadableArray txt, String implType, Promise promise) {
        try {
            getZeroconfImpl(implType).updateService(serviceName, txt, promise);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception while updating service: " + e.getMessage(), serviceName);
        }
    }

    @ReactMethod
    public void resolveService(String name, String type, String protocol, String domain, String implType, @Nullable ReadableMap options, Promise promise) {
        try {
            getZeroconfImpl(implType).resolveService(name, type, protocol, domain, ZeroconfOptions.from(options), promise);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception while resolving service: " + e.getMessage(), name);
        }
    }

    /**
     * Resolves "granted" or "denied". On Android 17 (API 37) devices, the ACCESS_LOCAL_NETWORK runtime permission
     * is enforced for apps targeting API 37 and for apps declaring it in their manifest. Otherwise there is nothing to grant.
     */
    @ReactMethod
    public void checkLocalNetworkAccess(Promise promise) {
        Context context = getReactApplicationContext();
        boolean required = Build.VERSION.SDK_INT >= 37
                && (context.getApplicationInfo().targetSdkVersion >= 37 || declaresPermission(context, PERMISSION_ACCESS_LOCAL_NETWORK));
        if (!required) {
            promise.resolve("granted");
            return;
        }
        boolean granted = context.checkSelfPermission(PERMISSION_ACCESS_LOCAL_NETWORK) == PackageManager.PERMISSION_GRANTED;
        promise.resolve(granted ? "granted" : "denied");
    }

    @ReactMethod
    public void unregisterService(String serviceName, String implType, Promise promise) {
        try {
            getZeroconfImpl(implType).unregisterService(serviceName, promise);
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
            String message = "Exception while unregistering service: " + e.getMessage();
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, serviceName);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, serviceName);
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
        sendError(domain, code, message, serviceName, null);
    }

    /**
     * @param scanId the scan the error belongs to, null for errors not related to a scan
     */
    public void sendError(String domain, Object code, String message, @Nullable String serviceName, @Nullable String scanId) {
        WritableMap error = buildError(domain, code, message, serviceName);
        if (scanId != null) {
            error.putString(KEY_SCAN_ID, scanId);
        }
        sendEvent(getReactApplicationContext(), EVENT_ERROR, error);
    }

    /**
     * Rejects a promise with the same { message, code, domain, serviceName } shape as error events, in userInfo
     */
    public static void reject(@Nullable Promise promise, String domain, Object code, String message, @Nullable String serviceName) {
        if (promise != null) {
            promise.reject(String.valueOf(code), message, buildError(domain, code, message, serviceName));
        }
    }

    private static WritableMap buildError(String domain, Object code, String message, @Nullable String serviceName) {
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
        return error;
    }

    public static final String SERVICE_TYPES_TYPE = "services._dns-sd";
    public static final String SERVICE_TYPES_PROTOCOL = "udp";

    /**
     * Whether a scan browses _services._dns-sd._udp, whose results are service types
     */
    public static boolean isServiceTypesScan(String type, String protocol) {
        return SERVICE_TYPES_TYPE.equals(type) && SERVICE_TYPES_PROTOCOL.equals(protocol);
    }

    /**
     * Service type from a _services._dns-sd._udp result: "_http" and "_tcp.local." give "_http._tcp"
     */
    public static String serviceTypeFromResult(String name, @Nullable String regType) {
        if (regType != null) {
            for (String label : regType.split("\\.")) {
                if (label.startsWith("_")) {
                    return name + "." + label;
                }
            }
        }
        return name;
    }

    private static boolean declaresPermission(Context context, String permission) {
        try {
            String[] requested = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), PackageManager.GET_PERMISSIONS).requestedPermissions;
            if (requested != null) {
                for (String declared : requested) {
                    if (permission.equals(declared)) {
                        return true;
                    }
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            // Our own package is always found
        }
        return false;
    }

    /**
     * The Android network of a network interface ("wlan0"), null when there is none
     */
    @Nullable
    public static Network findNetwork(Context context, String interfaceName) {
        ConnectivityManager connectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) {
            return null;
        }
        for (Network network : connectivityManager.getAllNetworks()) {
            LinkProperties properties = connectivityManager.getLinkProperties(network);
            if (properties != null && interfaceName.equals(properties.getInterfaceName())) {
                return network;
            }
        }
        return null;
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
            case 7: return "permission denied, the app needs the ACCESS_LOCAL_NETWORK permission"; // FAILURE_PERMISSION_DENIED (API 37)
            default: return "error " + errorCode;
        }
    }

    /**
     * Readable description of a DNSServiceErrorType code, as on iOS
     */
    public static String describeDnssdError(int errorCode) {
        switch (errorCode) {
            case -65548: return "name already in use"; // kDNSServiceErr_NameConflict
            case -65540: return "bad parameter"; // kDNSServiceErr_BadParam
            case -65538: return "no such name"; // kDNSServiceErr_NoSuchName
            case -65554: return "no such record"; // kDNSServiceErr_NoSuchRecord
            case -65563: return "mDNSResponder is not running"; // kDNSServiceErr_ServiceNotRunning
            case -65539: return "out of memory"; // kDNSServiceErr_NoMemory
            case -65544: return "unsupported"; // kDNSServiceErr_Unsupported
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
                impl.stopAll();
                impl.unregisterAllServices();
            }
        } catch (Throwable e) {
            Log.e(getClass().getName(), e.getMessage(), e);
        }
    }
}
