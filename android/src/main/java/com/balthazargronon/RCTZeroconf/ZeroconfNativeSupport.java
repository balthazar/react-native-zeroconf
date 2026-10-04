package com.balthazargronon.RCTZeroconf;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.util.Log;

import com.facebook.proguard.annotations.DoNotStrip;

/**
 * Android services for the C++ module (cpp/android): it calls these through JNI.
 */
@DoNotStrip
public final class ZeroconfNativeSupport {
    private static final String TAG = "ZeroconfNativeSupport";

    private static WifiManager.MulticastLock multicastLock;

    private ZeroconfNativeSupport() {}

    /**
     * Android drops multicast packets unless an app holds a multicast lock: held while scans, resolves
     * or published services are active, released when there are none.
     */
    @DoNotStrip
    public static synchronized void setMulticastLock(boolean held) {
        if (held) {
            if (multicastLock == null) {
                Context context = application();
                if (context == null) {
                    Log.w(TAG, "No application context, multicast stays filtered");
                    return;
                }
                @SuppressLint("WifiManagerLeak")
                WifiManager wifi = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
                if (wifi == null) {
                    return;
                }
                multicastLock = wifi.createMulticastLock("react-native-zeroconf");
                multicastLock.setReferenceCounted(false);
            }
            multicastLock.acquire();
        } else if (multicastLock != null && multicastLock.isHeld()) {
            multicastLock.release();
        }
    }

    // The application, without a ReactContext: the C++ module has none
    static Context application() {
        try {
            @SuppressLint("PrivateApi")
            Class<?> activityThread = Class.forName("android.app.ActivityThread");
            return (Application) activityThread.getMethod("currentApplication").invoke(null);
        } catch (Exception e) {
            Log.w(TAG, "Cannot get the application", e);
            return null;
        }
    }
}
