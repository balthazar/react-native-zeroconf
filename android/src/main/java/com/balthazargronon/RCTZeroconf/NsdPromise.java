package com.balthazargronon.RCTZeroconf;

import com.facebook.proguard.annotations.DoNotStrip;

import javax.annotation.Nullable;

/**
 * Settles a call of the C++ module (cpp/android/NsdBackend) once: with a service, or with
 * { domain, code, message, serviceName }.
 */
@DoNotStrip
public final class NsdPromise {
    private final long id;
    private boolean settled = false;

    @DoNotStrip
    public NsdPromise(long id) {
        this.id = id;
    }

    public synchronized void resolve(@Nullable NsdPayload service) {
        if (settled) {
            return;
        }
        settled = true;
        nativeResolve(id, service);
    }

    public synchronized void reject(String domain, String code, String message, @Nullable String serviceName) {
        if (settled) {
            return;
        }
        settled = true;
        nativeReject(id, domain, code, message, serviceName == null ? "" : serviceName);
    }

    private static native void nativeResolve(long id, @Nullable NsdPayload service);

    private static native void nativeReject(long id, String domain, String code, String message, String serviceName);
}
