package com.balthazargronon.RCTZeroconf.nsd;

import android.annotation.TargetApi;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;

/**
 * Follows found services with NsdManager.registerServiceInfoCallback (Android 14+) instead of one-shot resolves:
 * services can be followed concurrently, updates (addresses, TXT records) are delivered as they change.
 * Only loaded on API 34+, so older devices never touch these APIs.
 */
@TargetApi(34)
class ServiceInfoCallbacks {
    private static final String TAG = "ServiceInfoCallbacks";

    interface Listener {
        void onServiceUpdated(NsdServiceInfo serviceInfo);

        void onRegistrationFailed(NsdServiceInfo serviceInfo, int errorCode);
    }

    private final NsdManager nsdManager;
    private final Executor executor;
    private final Listener listener;
    private final Map<String, NsdManager.ServiceInfoCallback> callbacks = new ConcurrentHashMap<>();

    ServiceInfoCallbacks(NsdManager nsdManager, Executor executor, Listener listener) {
        this.nsdManager = nsdManager;
        this.executor = executor;
        this.listener = listener;
    }

    void register(final NsdServiceInfo serviceInfo) {
        final String name = serviceInfo.getServiceName();
        if (callbacks.containsKey(name)) {
            return;
        }

        NsdManager.ServiceInfoCallback callback = new NsdManager.ServiceInfoCallback() {
            @Override
            public void onServiceInfoCallbackRegistrationFailed(int errorCode) {
                callbacks.remove(name);
                listener.onRegistrationFailed(serviceInfo, errorCode);
            }

            @Override
            public void onServiceUpdated(NsdServiceInfo updated) {
                listener.onServiceUpdated(updated);
            }

            @Override
            public void onServiceLost() {
                // Reported by the discovery listener
            }

            @Override
            public void onServiceInfoCallbackUnregistered() {
            }
        };
        callbacks.put(name, callback);
        Log.d(TAG, "Following " + name + " with registerServiceInfoCallback");
        nsdManager.registerServiceInfoCallback(serviceInfo, executor, callback);
    }

    void unregister(String name) {
        NsdManager.ServiceInfoCallback callback = callbacks.remove(name);
        if (callback == null) {
            return;
        }
        try {
            nsdManager.unregisterServiceInfoCallback(callback);
        } catch (IllegalArgumentException e) {
            // Registration failed or was already unregistered
            Log.w(TAG, "unregisterServiceInfoCallback failed", e);
        }
    }

    void unregisterAll() {
        for (String name : new ArrayList<>(callbacks.keySet())) {
            unregister(name);
        }
    }
}
