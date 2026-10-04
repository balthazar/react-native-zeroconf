package com.balthazargronon.RCTZeroconf;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.os.Build;
import android.util.Log;

import com.balthazargronon.RCTZeroconf.nsd.NsdServiceImpl;
import com.facebook.proguard.annotations.DoNotStrip;

import java.util.LinkedHashMap;
import java.util.Map;

import javax.annotation.Nullable;

/**
 * Android NSD for the C++ module (cpp/android/NsdBackend): it calls these methods through JNI, events,
 * errors and results come back through native methods. Also the constants and helpers of the NSD code.
 */
@DoNotStrip
public final class NsdHost {
    private static final String TAG = "NsdHost";

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
    public static final String ERROR_DOMAIN_LIBRARY = "RNZeroconf";
    public static final String ERROR_CODE_EXCEPTION = "EXCEPTION";
    public static final String ERROR_CODE_TIMEOUT = "TIMEOUT";
    public static final String ERROR_CODE_UNKNOWN_INTERFACE = "UNKNOWN_INTERFACE";
    public static final String ERROR_CODE_UNSUPPORTED = "UNSUPPORTED";
    // Manifest.permission.ACCESS_LOCAL_NETWORK, API 37
    public static final String PERMISSION_ACCESS_LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK";
    public static final String ERROR_CODE_NOT_PUBLISHED = "NOT_PUBLISHED";

    // The C++ backend's id, 0 once it is gone
    private volatile long handle;
    private final Context context;
    private final NsdServiceImpl nsd;

    @DoNotStrip
    public NsdHost(long handle) {
        this.handle = handle;
        this.context = ZeroconfNativeSupport.application();
        this.nsd = new NsdServiceImpl(this, context);
    }

    public Context getContext() {
        return context;
    }

    @DoNotStrip
    public void scan(String scanId, String type, String protocol, String domain, String subtype, String networkInterface, double resolveTimeout) {
        try {
            nsd.scan(scanId, type, protocol, domain, new ZeroconfOptions(subtype, null, networkInterface, 5, resolveTimeout));
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage(), e);
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception during scan: " + e.getMessage(), null, scanId);
        }
    }

    // Stops the scan with this id, or every scan without one
    @DoNotStrip
    public void stop(String scanId) {
        try {
            if (scanId == null || scanId.isEmpty()) {
                nsd.stopAll();
            } else {
                nsd.stop(scanId);
            }
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage(), e);
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception during stop: " + e.getMessage(), null, scanId);
        }
    }

    @DoNotStrip
    public void registerService(String type, String protocol, String domain, String name, int port, String[] txtKeys, String[] txtValues,
                                String[] subtypes, String networkInterface, NsdPromise promise) {
        try {
            nsd.registerService(type, protocol, domain, name, port, txt(txtKeys, txtValues),
                    new ZeroconfOptions(null, subtypes, networkInterface, 5, 5), promise);
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage(), e);
            String message = "Exception while registering service: " + e.getMessage();
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, name);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, name);
        }
    }

    @DoNotStrip
    public void updateService(String name, String[] txtKeys, String[] txtValues, NsdPromise promise) {
        try {
            nsd.updateService(name, txt(txtKeys, txtValues), promise);
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage(), e);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception while updating service: " + e.getMessage(), name);
        }
    }

    @DoNotStrip
    public void unregisterService(String name, NsdPromise promise) {
        try {
            nsd.unregisterService(name, promise);
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage(), e);
            String message = "Exception while unregistering service: " + e.getMessage();
            sendError(ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, name);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, message, name);
        }
    }

    @DoNotStrip
    public void resolveService(String name, String type, String protocol, String domain, String networkInterface, double timeout, NsdPromise promise) {
        try {
            nsd.resolveService(name, type, protocol, domain, new ZeroconfOptions(null, null, networkInterface, timeout, 5), promise);
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage(), e);
            reject(promise, ERROR_DOMAIN_LIBRARY, ERROR_CODE_EXCEPTION, "Exception while resolving service: " + e.getMessage(), name);
        }
    }

    // Stops the scans and unpublishes the services, when the module goes away
    @DoNotStrip
    public void shutdown() {
        try {
            nsd.stopAll();
            nsd.unregisterAllServices();
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage(), e);
        }
        handle = 0;
    }

    /**
     * "granted" or "denied". On Android 17 (API 37) devices, the ACCESS_LOCAL_NETWORK runtime permission
     * is enforced for apps targeting API 37 and for apps declaring it in their manifest. Otherwise there is nothing to grant.
     */
    @DoNotStrip
    public static String checkLocalNetworkAccess() {
        Context context = ZeroconfNativeSupport.application();
        if (context == null) {
            return "unknown";
        }
        boolean required = Build.VERSION.SDK_INT >= 37
                && (context.getApplicationInfo().targetSdkVersion >= 37 || declaresPermission(context, PERMISSION_ACCESS_LOCAL_NETWORK));
        if (!required) {
            return "granted";
        }
        boolean granted = context.checkSelfPermission(PERMISSION_ACCESS_LOCAL_NETWORK) == PackageManager.PERMISSION_GRANTED;
        return granted ? "granted" : "denied";
    }

    private static Map<String, String> txt(String[] keys, String[] values) {
        Map<String, String> txt = new LinkedHashMap<>();
        for (int i = 0; i < keys.length && i < values.length; i++) {
            txt.put(keys[i], values[i]);
        }
        return txt;
    }

    // Events of the NSD code, with the scan id in the payload for scan events
    public void sendEvent(String eventName, NsdPayload body) {
        long current = handle;
        if (current != 0) {
            nativeEvent(current, eventName, body);
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
        long current = handle;
        if (current != 0) {
            nativeError(current, scanId == null ? "" : scanId, domain, String.valueOf(code), message, serviceName == null ? "" : serviceName);
        }
    }

    // Rejects a call with the same { message, code, domain, serviceName } as error events
    public static void reject(@Nullable NsdPromise promise, String domain, Object code, String message, @Nullable String serviceName) {
        if (promise != null) {
            promise.reject(domain, String.valueOf(code), message, serviceName);
        }
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

    private static native void nativeEvent(long handle, String eventName, NsdPayload body);

    private static native void nativeError(long handle, String scanId, String domain, String code, String message, String serviceName);
}
