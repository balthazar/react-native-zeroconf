package com.balthazargronon.RCTZeroconf;

import com.balthazargronon.RCTZeroconf.nsd.NsdServiceImpl;
import com.facebook.react.bridge.ReactApplicationContext;

import org.apache.commons.lang3.StringUtils;

public class ZeroConfImplFactory {
    public static final String NSD_IMPL = "NSD";

    private ZeroconfModule zeroconfModule;
    private ReactApplicationContext context;
    private Zeroconf nsdService;

    public ZeroConfImplFactory(ZeroconfModule zeroconfModule, ReactApplicationContext context) {
        this.zeroconfModule = zeroconfModule;
        this.context = context;
    }

    public Zeroconf getZeroconf() {
        return new NsdServiceImpl(zeroconfModule, context)
    }
}