package com.balthazargronon.RCTZeroconf;

import com.facebook.react.bridge.ReadableArray;

public interface Zeroconf {

    void scan(String type, String protocol, String domain);

    void stop();

    public void unregisterService(String serviceName);

    /**
     * @param txt TXT records as ordered [key, value] pairs
     */
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableArray txt);
}
