package com.balthazargronon.RCTZeroconf.nsd;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
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

import java.io.UnsupportedEncodingException;
import java.net.InetAddress;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class NsdServiceImpl implements Zeroconf {
    private static final String TAG = "NsdServiceImpl";
    
    private final NsdManager mNsdManager;
    private final WifiManager wifiManager;
    private NsdManager.DiscoveryListener mDiscoveryListener;
    private WifiManager.MulticastLock multicastLock;
    private final Map<String, NsdManager.RegistrationListener> mPublishedServices = new ConcurrentHashMap<>();
    private final ZeroconfModule zeroconfModule;
    private final ReactApplicationContext reactApplicationContext;

    public NsdServiceImpl(ZeroconfModule zeroconfModule, ReactApplicationContext reactContext) {
        this.zeroconfModule = zeroconfModule;
        this.reactApplicationContext = reactContext;
        this.mNsdManager = (NsdManager) reactContext.getSystemService(Context.NSD_SERVICE);
        this.wifiManager = (WifiManager) reactContext.getSystemService(Context.WIFI_SERVICE);
    }

    @Override
    public void scan(String type, String protocol, String domain) {
        stop(); // Ensure previous scans are stopped

        acquireMulticastLock();

        mDiscoveryListener = new NsdManager.DiscoveryListener() {
            @Override
            public void onStartDiscoveryFailed(String serviceType, int errorCode) {
                handleDiscoveryError("Start", errorCode);
            }

            @Override
            public void onStopDiscoveryFailed(String serviceType, int errorCode) {
                handleDiscoveryError("Stop", errorCode);
            }

            @Override
            public void onDiscoveryStarted(String serviceType) {
                Log.d(TAG, "Discovery Started");
                zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_START, null);
            }

            @Override
            public void onDiscoveryStopped(String serviceType) {
                Log.d(TAG, "Discovery Stopped");
                zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_STOP, null);
            }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service Found: " + serviceInfo.getServiceName());
                WritableMap service = new WritableNativeMap();
                service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());

                zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_FOUND, service);
                mNsdManager.resolveService(serviceInfo, new ZeroResolveListener());
            }

            @Override
            public void onServiceLost(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service Lost: " + serviceInfo.getServiceName());
                WritableMap service = new WritableNativeMap();
                service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());
                zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_REMOVE, service);
            }
        };

        String serviceType = String.format("_%s._%s.", type, protocol);
        mNsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
    }

    @Override
    public void stop() {
        if (mDiscoveryListener != null) {
            try {
                mNsdManager.stopServiceDiscovery(mDiscoveryListener);
                Log.d(TAG, "Stopped service discovery");
            } catch (IllegalArgumentException e) {
                Log.w(TAG, "Service discovery already stopped", e);
            }
            mDiscoveryListener = null;
        }

        releaseMulticastLock();
    }

    @Override
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt) {
        String serviceType = String.format("_%s._%s.", type, protocol);
        NsdServiceInfo serviceInfo = new NsdServiceInfo();
        serviceInfo.setServiceName(name);
        serviceInfo.setServiceType(serviceType);
        serviceInfo.setPort(port);

        ReadableMapKeySetIterator iterator = txt.keySetIterator();
        while (iterator.hasNextKey()) {
            String key = iterator.nextKey();
            serviceInfo.setAttribute(key, txt.getString(key));
        }

        NsdManager.RegistrationListener listener = new ServiceRegistrationListener();
        mNsdManager.registerService(serviceInfo, NsdManager.PROTOCOL_DNS_SD, listener);
    }

    @Override
    public void unregisterService(String serviceName) {
        NsdManager.RegistrationListener serviceListener = mPublishedServices.remove(serviceName);
        if (serviceListener != null) {
            mNsdManager.unregisterService(serviceListener);
            Log.d(TAG, "Unregistered service: " + serviceName);
        } else {
            Log.w(TAG, "Service not found for unregistration: " + serviceName);
        }
    }

    private void acquireMulticastLock() {
        if (multicastLock == null) {
            multicastLock = wifiManager.createMulticastLock("multicastLock");
            multicastLock.setReferenceCounted(true);
        }
        if (!multicastLock.isHeld()) {
            multicastLock.acquire();
            Log.d(TAG, "Multicast lock acquired");
        }
    }

    private void releaseMulticastLock() {
        if (multicastLock != null && multicastLock.isHeld()) {
            multicastLock.release();
            Log.d(TAG, "Multicast lock released");
            multicastLock = null;
        }
    }

    private void handleDiscoveryError(String action, int errorCode) {
        String error = String.format("Discovery %s failed with code: %d", action, errorCode);
        Log.e(TAG, error);
        zeroconfModule.sendEvent(reactApplicationContext, ZeroconfModule.EVENT_ERROR, error);
    }

    private ReactApplicationContext getReactApplicationContext() {
        return reactApplicationContext;
    }

    private class ZeroResolveListener implements NsdManager.ResolveListener {
        @Override
        public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
            if (errorCode == NsdManager.FAILURE_ALREADY_ACTIVE) {
                Log.w(TAG, "Resolve already active for service: " + serviceInfo.getServiceName());
                mNsdManager.resolveService(serviceInfo, this);
            } else {
                String error = "Resolving service failed with code: " + errorCode;
                Log.e(TAG, error);
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
            }
        }

        @Override
        public void onServiceResolved(NsdServiceInfo serviceInfo) {
            Log.d(TAG, "Service Resolved: " + serviceInfo.getServiceName());
            WritableMap service = serviceInfoToMap(serviceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_RESOLVE, service);
        }
    }

    private class ServiceRegistrationListener implements NsdManager.RegistrationListener {
        @Override
        public void onServiceRegistered(NsdServiceInfo serviceInfo) {
            String serviceName = serviceInfo.getServiceName();
            mPublishedServices.put(serviceName, this);
            Log.d(TAG, "Service Registered: " + serviceName);

            WritableMap service = serviceInfoToMap(serviceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_PUBLISHED, service);
        }

        @Override
        public void onRegistrationFailed(NsdServiceInfo serviceInfo, int errorCode) {
            String error = "Service registration failed for " + serviceInfo.getServiceName() + " with code: " + errorCode;
            Log.e(TAG, error);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
        }

        @Override
        public void onServiceUnregistered(NsdServiceInfo serviceInfo) {
            String serviceName = serviceInfo.getServiceName();
            Log.d(TAG, "Service Unregistered: " + serviceName);

            WritableMap service = serviceInfoToMap(serviceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_UNREGISTERED, service);
        }

        @Override
        public void onUnregistrationFailed(NsdServiceInfo serviceInfo, int errorCode) {
            String error = "Service unregistration failed for " + serviceInfo.getServiceName() + " with code: " + errorCode;
            Log.e(TAG, error);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
        }
    }

    private WritableMap serviceInfoToMap(NsdServiceInfo serviceInfo) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());

        InetAddress host = serviceInfo.getHost();
        if (host != null) {
            service.putString(ZeroconfModule.KEY_SERVICE_HOST, host.getHostName());

            WritableArray addresses = new WritableNativeArray();
            addresses.pushString(host.getHostAddress());
            service.putArray(ZeroconfModule.KEY_SERVICE_ADDRESSES, addresses);
        }

        String fullServiceName = (host != null) ? host.getHostName() + serviceInfo.getServiceType() : serviceInfo.getServiceName();
        service.putString(ZeroconfModule.KEY_SERVICE_FULL_NAME, fullServiceName);
        service.putInt(ZeroconfModule.KEY_SERVICE_PORT, serviceInfo.getPort());

        WritableMap txtRecords = new WritableNativeMap();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Map<String, byte[]> attributes = serviceInfo.getAttributes();
            if (attributes != null) {
                attributes.forEach((key, value) -> {
                    try {
                        String txtValue = (value != null) ? new String(value, "UTF-8") : "";
                        txtRecords.putString(key, txtValue);
                    } catch (UnsupportedEncodingException e) {
                        String error = "Failed to encode txtRecord for key " + key;
                        Log.e(TAG, error, e);
                        zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
                    }
                });
            }
        }
        service.putMap(ZeroconfModule.KEY_SERVICE_TXT, txtRecords);

        return service;
    }
}