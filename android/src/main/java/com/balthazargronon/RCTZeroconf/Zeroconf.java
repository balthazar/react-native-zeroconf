package com.balthazargronon.RCTZeroconf;

import com.facebook.react.bridge.ReadableMap;

public interface Zeroconf {

    void scan(String type, String protocol, String domain);

    void stop();

    public void unregisterService(String serviceName);

    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt);
}
