package com.balthazargronon.RCTZeroconf;

import android.util.Log;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;

import javax.annotation.Nullable;

/**
 * React Native module for ZeroConf (NSD) service discovery and registration.
 * Provides methods to scan, stop scanning, register, and unregister services.
 *
 * Created by Jeremy White on 8/1/2016.
 * Copyright © 2016 Balthazar Gronon MIT
 */
public class ZeroconfModule extends ReactContextBaseJavaModule {

    // Event names
    public static final String EVENT_START = "RNZeroconfStart";
    public static final String EVENT_STOP = "RNZeroconfStop";
    public static final String EVENT_ERROR = "RNZeroconfError";
    public static final String EVENT_FOUND = "RNZeroconfFound";
    public static final String EVENT_REMOVE = "RNZeroconfRemove";
    public static final String EVENT_RESOLVE = "RNZeroconfResolved";
    public static final String EVENT_PUBLISHED = "RNZeroconfServiceRegistered";
    public static final String EVENT_UNREGISTERED = "RNZeroconfServiceUnregistered";

    // Service info keys
    public static final String KEY_SERVICE_NAME = "name";
    public static final String KEY_SERVICE_FULL_NAME = "fullName";
    public static final String KEY_SERVICE_HOST = "host";
    public static final String KEY_SERVICE_PORT = "port";
    public static final String KEY_SERVICE_ADDRESSES = "addresses";
    public static final String KEY_SERVICE_TXT = "txt";

    // Logging tag
    private static final String TAG = "ZeroconfModule";

    // Factory for creating Zeroconf implementations
    private final ZeroConfImplFactory zeroConfFactory;

    /**
     * Constructor for ZeroconfModule.
     *
     * @param reactContext The React application context.
     */
    public ZeroconfModule(ReactApplicationContext reactContext) {
        super(reactContext);
        this.zeroConfFactory = new ZeroConfImplFactory(this, reactContext);
    }

    /**
     * Returns the name of this module. This is used to refer to the module from JavaScript.
     *
     * @return The name of the module.
     */
    @Override
    public String getName() {
        return "RNZeroconf";
    }

    /**
     * Initiates a scan for services with the specified type, protocol, and domain.
     *
     * @param type     The service type (e.g., "http").
     * @param protocol The protocol (e.g., "tcp").
     * @param domain   The domain (e.g., "local").
     */
    @ReactMethod
    public void scan(String type, String protocol, String domain) {
        executeZeroconfAction(() -> {
            Zeroconf zeroconf = getZeroconfImpl();
            zeroconf.scan(type, protocol, domain);
        }, "Exception During Scan");
    }

    /**
     * Stops any ongoing service discovery.
     */
    @ReactMethod
    public void stop() {
        executeZeroconfAction(() -> {
            Zeroconf zeroconf = getZeroconfImpl();
            zeroconf.stop();
        }, "Exception During Stop");
    }

    /**
     * Registers a service with the specified parameters.
     *
     * @param type     The service type (e.g., "http").
     * @param protocol The protocol (e.g., "tcp").
     * @param domain   The domain (e.g., "local").
     * @param name     The service name.
     * @param port     The service port.
     * @param txt      TXT records as a map.
     */
    @ReactMethod
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt) {
        executeZeroconfAction(() -> {
            Zeroconf zeroconf = getZeroconfImpl();
            zeroconf.registerService(type, protocol, domain, name, port, txt);
        }, "Exception During Register Service");
    }

    /**
     * Unregisters a previously registered service by its name.
     *
     * @param serviceName The name of the service to unregister.
     */
    @ReactMethod
    public void unregisterService(String serviceName) {
        executeZeroconfAction(() -> {
            Zeroconf zeroconf = getZeroconfImpl();
            zeroconf.unregisterService(serviceName);
        }, "Exception During Unregister Service");
    }

    /**
     * Sends an event to the JavaScript side.
     *
     * @param eventName The name of the event.
     * @param params    The event parameters.
     */
    private void sendEvent(String eventName, @Nullable Object params) {
        ReactApplicationContext reactContext = getReactApplicationContext();
        if (reactContext.hasActiveCatalystInstance()) {
            reactContext
                    .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                    .emit(eventName, params);
        } else {
            Log.w(TAG, "ReactContext does not have an active Catalyst instance. Event not sent: " + eventName);
        }
    }

    /**
     * Executes a Zeroconf action with standardized error handling.
     *
     * @param action       The Zeroconf action to execute.
     * @param errorContext The context string for error messages.
     */
    private void executeZeroconfAction(Runnable action, String errorContext) {
        try {
            action.run();
        } catch (Exception e) {
            Log.e(TAG, errorContext + ": " + e.getMessage(), e);
            sendEvent(EVENT_ERROR, errorContext + ": " + e.getMessage());
        }
    }

    /**
     * Retrieves the Zeroconf implementation from the factory.
     *
     * @return The Zeroconf instance.
     */
    private Zeroconf getZeroconfImpl() {
        return zeroConfFactory.getZeroconf();
    }

    /**
     * Cleans up resources when the Catalyst instance is destroyed.
     */
    @Override
    public void onCatalystInstanceDestroy() {
        super.onCatalystInstanceDestroy();
        try {
            stop();
        } catch (Exception e) {
            Log.e(TAG, "Exception During Catalyst Destroy: " + e.getMessage(), e);
            sendEvent(EVENT_ERROR, "Exception During Catalyst Destroy: " + e.getMessage());
        }
    }
}