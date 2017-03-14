package com.balthazargronon.RCTZeroconf;

import android.content.Context;
import android.net.nsd.NsdServiceInfo;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeArray;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;
import com.youview.tinydnssd.MDNSDiscover;

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

    public static final String KEY_SERVICE_NAME = "name";
    public static final String KEY_SERVICE_FULL_NAME = "fullName";
    public static final String KEY_SERVICE_HOST = "host";
    public static final String KEY_SERVICE_PORT = "port";
    public static final String KEY_SERVICE_ADDRESSES = "addresses";

    protected DiscoverResolver mDiscoverResolver;

    public ZeroconfModule(ReactApplicationContext reactContext) {
        super(reactContext);
    }

    @Override
    public String getName() {
        return "RNZeroconf";
    }

    @ReactMethod
    public void scan(String type, String protocol, String domain) {
        String serviceType = String.format("_%s._%s.", type, protocol);

        if (mDiscoverResolver == null) {
            mDiscoverResolver = new MyDiscoverResolver(getReactApplicationContext(), serviceType, 1000);
        }

        this.stop();

        mDiscoverResolver.start();
    }

    @ReactMethod
    public void stop() {
        if (mDiscoverResolver != null) {
            mDiscoverResolver.stop();
        }

        mDiscoverResolver = null;
    }

    protected void sendEvent(ReactContext reactContext,
                             String eventName,
                             @Nullable Object params) {
        reactContext
                .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                .emit(eventName, params);
    }

    private class MyDiscoverResolver extends DiscoverResolver {
        MyDiscoverResolver(Context context, String serviceType, int debounceMillis) {
            super(context, serviceType, debounceMillis);
        }

        @Override
        public void onDiscoveryStarted() {
            sendEvent(getReactApplicationContext(), EVENT_START, null);
        }

        @Override
        public void onDiscoveryStopped() {
            sendEvent(getReactApplicationContext(), EVENT_STOP, null);
        }

        @Override
        public void onServiceFound(NsdServiceInfo serviceInfo) {
            WritableMap service = new WritableNativeMap();
            service.putString(KEY_SERVICE_NAME, serviceInfo.getServiceName());

            sendEvent(getReactApplicationContext(), EVENT_FOUND, service);
        }

        @Override
        public void onServiceLost(String serviceName, MDNSDiscover.Result serviceResult) {
            WritableMap service = new WritableNativeMap();
            service.putString(KEY_SERVICE_NAME, serviceName);
            service.putString(KEY_SERVICE_FULL_NAME, serviceResult.srv.fqdn);
            sendEvent(getReactApplicationContext(), EVENT_REMOVE, service);
        }

        @Override
        public void onServiceResolved(String serviceName, MDNSDiscover.Result serviceResult) {
            WritableMap service = new WritableNativeMap();
            service.putString(KEY_SERVICE_NAME, serviceName);
            service.putString(KEY_SERVICE_FULL_NAME, serviceResult.srv.fqdn);
            service.putString(KEY_SERVICE_HOST, serviceResult.srv.target);
            service.putInt(KEY_SERVICE_PORT, serviceResult.srv.port);

            WritableArray addresses = new WritableNativeArray();
            addresses.pushString(serviceResult.a.ipaddr);

            service.putArray(KEY_SERVICE_ADDRESSES, addresses);

            sendEvent(getReactApplicationContext(), EVENT_RESOLVE, service);
        }

        @Override
        public void onStartDiscoveryFailed(int errorCode) {
            String error = "Starting service discovery failed with code: " + errorCode;
            sendEvent(getReactApplicationContext(), EVENT_ERROR, error);
        }

        @Override
        public void onStopDiscoveryFailed(int errorCode) {
            String error = "Stopping service discovery failed with code: " + errorCode;
            sendEvent(getReactApplicationContext(), EVENT_ERROR, error);
        }

    }

    @Override
    public void onCatalystInstanceDestroy() {
        super.onCatalystInstanceDestroy();
        stop();
    }
}
