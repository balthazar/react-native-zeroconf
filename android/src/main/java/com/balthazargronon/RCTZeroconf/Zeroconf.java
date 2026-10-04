package com.balthazargronon.RCTZeroconf;


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
    public void unregisterService(String serviceName, @Nullable NsdPromise promise);

    /**
     * @param txt TXT records as ordered [key, value] pairs
     * @param promise resolved with the service once registered
     */
    public void registerService(String type, String protocol, String domain, String name, int port, java.util.Map<String, String> txt, ZeroconfOptions options, NsdPromise promise);

    /**
     * Replaces the TXT record of a published service
     */
    void updateService(String serviceName, java.util.Map<String, String> txt, NsdPromise promise);

    /**
     * Resolves one service by name, without scanning
     */
    void resolveService(String name, String type, String protocol, String domain, ZeroconfOptions options, NsdPromise promise);

    /**
     * Unregister every service published through this implementation
     */
    void unregisterAllServices();
}
