package com.balthazargronon.RCTZeroconf;

import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReadableArray;

import javax.annotation.Nullable;

public interface Zeroconf {

    /**
     * @param scanId id of the JS instance starting the scan, several scans can run at once
     */
    void scan(String scanId, String type, String protocol, String domain, ZeroconfOptions options);

    void stop(String scanId);

    void stopAll();

    /**
     * @param promise resolved with the service once unregistered, null when nobody waits for it
     */
    public void unregisterService(String serviceName, @Nullable Promise promise);

    /**
     * @param txt TXT records as ordered [key, value] pairs
     * @param promise resolved with the service once registered
     */
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableArray txt, ZeroconfOptions options, Promise promise);

    /**
     * Replaces the TXT record of a published service
     */
    void updateService(String serviceName, ReadableArray txt, Promise promise);

    /**
     * Resolves one service by name, without scanning
     */
    void resolveService(String name, String type, String protocol, String domain, ZeroconfOptions options, Promise promise);

    /**
     * Unregister every service published through this implementation
     */
    void unregisterAllServices();
}
