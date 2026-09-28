package com.balthazargronon.RCTZeroconf.nsd;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
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

import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class NsdServiceImpl implements Zeroconf {
    private static final String TAG = "NsdServiceImpl";
    private static final long RESOLVE_RETRY_DELAY_MS = 100;

    private NsdManager mNsdManager;
    private NsdManager.DiscoveryListener mDiscoveryListener;
    private WifiManager.MulticastLock multicastLock;
    private Map<String, NsdManager.RegistrationListener> mPublishedServices;
    private ZeroconfModule zeroconfModule;
    private ReactApplicationContext reactApplicationContext;

    // NsdManager can only resolve one service at a time before API 34, so resolves are queued
    private final ArrayDeque<NsdServiceInfo> mResolveQueue = new ArrayDeque<>();
    private boolean mIsResolving = false;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    public NsdServiceImpl(ZeroconfModule zeroconfModule, ReactApplicationContext reactApplicationContext) {
        this.zeroconfModule = zeroconfModule;
        this.reactApplicationContext = reactApplicationContext;
        mPublishedServices = new ConcurrentHashMap<String, NsdManager.RegistrationListener>();
    }

    @Override
    public void scan(String type, String protocol, String domain) {
        if (mNsdManager == null) {
            mNsdManager = (NsdManager) getReactApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        this.stop();

        if (multicastLock == null) {
            @SuppressLint("WifiManagerLeak") WifiManager wifi = (WifiManager) getReactApplicationContext().getSystemService(Context.WIFI_SERVICE);
            multicastLock = wifi.createMulticastLock("multicastLock");
            multicastLock.setReferenceCounted(true);
            multicastLock.acquire();
        }

        mDiscoveryListener = new NsdManager.DiscoveryListener() {
            @Override
            public void onStartDiscoveryFailed(String serviceType, int errorCode) {
                String error = "Starting service discovery failed with code: " + errorCode;
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
            }

            @Override
            public void onStopDiscoveryFailed(String serviceType, int errorCode) {
                String error = "Stopping service discovery failed with code: " + errorCode;
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
            }

            @Override
            public void onDiscoveryStarted(String serviceType) {
                Log.d(TAG, "Discovery started");
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_START, null);
            }

            @Override
            public void onDiscoveryStopped(String serviceType) {
                Log.d(TAG, "Discovery stopped");
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_STOP, null);
            }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service found");
                WritableMap service = new WritableNativeMap();
                service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());

                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_FOUND, service);
                enqueueResolve(serviceInfo);
            }

            @Override
            public void onServiceLost(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service lost");
                WritableMap service = new WritableNativeMap();
                service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_REMOVE, service);
            }
        };

        String serviceType = String.format("_%s._%s.", type, protocol);
        mNsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
    }

    @Override
    public void stop() {
        if (mDiscoveryListener != null && mNsdManager != null) {
            try {
                mNsdManager.stopServiceDiscovery(mDiscoveryListener);
            } catch (IllegalArgumentException e) {
                // Listener was never registered or already unregistered (e.g. discovery failed to start)
                Log.w(TAG, "stopServiceDiscovery failed", e);
            }
        }

        mHandler.removeCallbacksAndMessages(null);
        synchronized (mResolveQueue) {
            mResolveQueue.clear();
            mIsResolving = false;
        }

        if (multicastLock != null) {
            multicastLock.release();
        }

        mDiscoveryListener = null;
        multicastLock = null;
    }

    @Override
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt) {
        String serviceType = String.format("_%s._%s.", type, protocol);

        final NsdManager nsdManager = this.getNsdManager();
        NsdServiceInfo serviceInfo  = new NsdServiceInfo();
        serviceInfo.setServiceName(name);
        serviceInfo.setServiceType(serviceType);
        serviceInfo.setPort(port);

        ReadableMapKeySetIterator iterator = txt.keySetIterator();
        while (iterator.hasNextKey()) {
            String key = iterator.nextKey();
            serviceInfo.setAttribute(key, txt.getString(key));
        }

        nsdManager.registerService(
                serviceInfo, NsdManager.PROTOCOL_DNS_SD, new ServiceRegistrationListener());
    }

    @Override
    public void unregisterService(String serviceName) {

        final NsdManager nsdManager = this.getNsdManager();

        NsdManager.RegistrationListener serviceListener = mPublishedServices.get(serviceName);

        if (serviceListener != null) {
            mPublishedServices.remove(serviceName);
            nsdManager.unregisterService(serviceListener);
        }
    }

    private NsdManager getNsdManager() {
        if (mNsdManager == null) {
            mNsdManager = (NsdManager) getReactApplicationContext().getSystemService(Context.NSD_SERVICE);
        }
        return mNsdManager;
    }

    private ReactApplicationContext getReactApplicationContext() {
        return reactApplicationContext;
    }

    private void enqueueResolve(NsdServiceInfo serviceInfo) {
        synchronized (mResolveQueue) {
            mResolveQueue.add(serviceInfo);
        }
        resolveNext();
    }

    private void resolveNext() {
        NsdServiceInfo next;
        synchronized (mResolveQueue) {
            if (mIsResolving || mResolveQueue.isEmpty()) {
                return;
            }
            next = mResolveQueue.poll();
            mIsResolving = true;
        }
        try {
            getNsdManager().resolveService(next, new ZeroResolveListener());
        } catch (Throwable e) {
            Log.e(TAG, "resolveService failed", e);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, "Resolving service failed: " + e.getMessage());
            onResolveDone();
        }
    }

    private void onResolveDone() {
        synchronized (mResolveQueue) {
            mIsResolving = false;
        }
        resolveNext();
    }

    private class ZeroResolveListener implements NsdManager.ResolveListener {
        @Override
        public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
            if (errorCode == NsdManager.FAILURE_ALREADY_ACTIVE) {
                // Another resolve (possibly from another app or listener) is in flight: retry shortly
                synchronized (mResolveQueue) {
                    mResolveQueue.addFirst(serviceInfo);
                    mIsResolving = false;
                }
                mHandler.postDelayed(NsdServiceImpl.this::resolveNext, RESOLVE_RETRY_DELAY_MS);
                return;
            }
            String error = "Resolving service failed with code: " + errorCode;
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
            onResolveDone();
        }

        @Override
        public void onServiceResolved(NsdServiceInfo serviceInfo) {
            WritableMap service = serviceInfoToMap(serviceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_RESOLVE, service);
            onResolveDone();
        }
    }

    private class ServiceRegistrationListener implements NsdManager.RegistrationListener {

        @Override
        public void onServiceRegistered(NsdServiceInfo NsdServiceInfo) {
            // Save the service name.  Android may have changed it in order to
            // resolve a conflict, so update the name you initially requested
            // with the name Android actually used.

            final String serviceName = NsdServiceInfo.getServiceName();
            mPublishedServices.put(serviceName, this);

            WritableMap service = serviceInfoToMap(NsdServiceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_PUBLISHED, service);
        }

        @Override
        public void onRegistrationFailed(NsdServiceInfo serviceInfo, int errorCode) {
            String error = "Registering service " + serviceInfo.getServiceName() + " failed with code: " + errorCode;
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
        }

        @Override
        public void onServiceUnregistered(NsdServiceInfo nsdServiceInfo) {
            // Service has been unregistered.  This only happens when you call
            // NsdManager.unregisterService() and pass in this listener.
            final WritableMap service = serviceInfoToMap(nsdServiceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_UNREGISTERED, service);
        }

        @Override
        public void onUnregistrationFailed(NsdServiceInfo serviceInfo, int errorCode) {
            String error = "Unregistering service " + serviceInfo.getServiceName() + " failed with code: " + errorCode;
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
        }
    }

    private WritableMap serviceInfoToMap(NsdServiceInfo serviceInfo) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());
        final InetAddress host = serviceInfo.getHost();
        final String fullServiceName;
        if (host == null) {
            fullServiceName = serviceInfo.getServiceName();
        } else {
            fullServiceName = host.getHostName() + serviceInfo.getServiceType();
            service.putString(ZeroconfModule.KEY_SERVICE_HOST, host.getHostName());

            WritableArray addresses = new WritableNativeArray();
            addresses.pushString(host.getHostAddress());

            service.putArray(ZeroconfModule.KEY_SERVICE_ADDRESSES, addresses);
        }
        service.putString(ZeroconfModule.KEY_SERVICE_FULL_NAME, fullServiceName);
        service.putInt(ZeroconfModule.KEY_SERVICE_PORT, serviceInfo.getPort());

        WritableMap txtRecords = new WritableNativeMap();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Map<String, byte[]> attributes = serviceInfo.getAttributes();
            if (attributes != null) {
                for (String key : attributes.keySet()) {
                    byte[] recordValue = attributes.get(key);
                    txtRecords.putString(String.format(Locale.getDefault(), "%s", key), recordValue != null ? new String(recordValue, StandardCharsets.UTF_8) : "");
                }
            }
        }

        service.putMap(ZeroconfModule.KEY_SERVICE_TXT, txtRecords);

        return service;
    }
}
