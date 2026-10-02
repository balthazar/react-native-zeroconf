package com.balthazargronon.RCTZeroconf.nsd;

import android.annotation.TargetApi;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.Handler;
import android.util.Log;

import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;

/**
 * Follows found services with NsdManager.registerServiceInfoCallback (Android 14+) instead of one-shot resolves:
 * services can be followed concurrently, updates (addresses, TXT records) are delivered as they change.
 * A service without addresses after the resolve timeout is followed again once, then reported with onTimeout.
 * Only loaded on API 34+, so older devices never touch these APIs.
 */
@TargetApi(34)
class ServiceInfoCallbacks {
    private static final String TAG = "ServiceInfoCallbacks";

    interface Listener {
        void onServiceUpdated(NsdServiceInfo serviceInfo);

        void onRegistrationFailed(NsdServiceInfo serviceInfo, int errorCode);

        void onTimeout(NsdServiceInfo serviceInfo);
    }

    private static class Followed {
        NsdManager.ServiceInfoCallback callback;
        Runnable timeout;
        volatile boolean resolved;
    }

    private final NsdManager nsdManager;
    private final Executor executor;
    private final Handler handler;
    private final long timeoutMs;
    private final Listener listener;
    private final Map<String, Followed> followed = new ConcurrentHashMap<>();

    ServiceInfoCallbacks(NsdManager nsdManager, Executor executor, Handler handler, long timeoutMs, Listener listener) {
        this.nsdManager = nsdManager;
        this.executor = executor;
        this.handler = handler;
        this.timeoutMs = timeoutMs;
        this.listener = listener;
    }

    void register(final NsdServiceInfo serviceInfo) {
        if (followed.containsKey(serviceInfo.getServiceName())) {
            return;
        }
        follow(serviceInfo, false);
    }

    private void follow(final NsdServiceInfo serviceInfo, final boolean retried) {
        final String name = serviceInfo.getServiceName();
        final Followed entry = new Followed();
        entry.callback = new NsdManager.ServiceInfoCallback() {
            @Override
            public void onServiceInfoCallbackRegistrationFailed(int errorCode) {
                if (followed.remove(name, entry)) {
                    handler.removeCallbacks(entry.timeout);
                    listener.onRegistrationFailed(serviceInfo, errorCode);
                }
            }

            @Override
            public void onServiceUpdated(NsdServiceInfo updated) {
                if (!updated.getHostAddresses().isEmpty()) {
                    entry.resolved = true;
                }
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
        entry.timeout = () -> {
            if (followed.get(name) != entry || entry.resolved) {
                return;
            }
            unregister(name);
            // Slow devices can time out, follow again once before reporting the error
            if (!retried) {
                follow(serviceInfo, true);
            } else {
                listener.onTimeout(serviceInfo);
            }
        };
        followed.put(name, entry);
        Log.d(TAG, "Following " + name + " with registerServiceInfoCallback");
        nsdManager.registerServiceInfoCallback(serviceInfo, executor, entry.callback);
        handler.postDelayed(entry.timeout, timeoutMs);
    }

    void unregister(String name) {
        Followed entry = followed.remove(name);
        if (entry == null) {
            return;
        }
        handler.removeCallbacks(entry.timeout);
        try {
            nsdManager.unregisterServiceInfoCallback(entry.callback);
        } catch (IllegalArgumentException e) {
            // Registration failed or was already unregistered
            Log.w(TAG, "unregisterServiceInfoCallback failed", e);
        }
    }

    void unregisterAll() {
        for (String name : new ArrayList<>(followed.keySet())) {
            unregister(name);
        }
    }
}
