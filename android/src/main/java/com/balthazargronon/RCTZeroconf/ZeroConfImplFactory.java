package com.balthazargronon.RCTZeroconf;

import com.balthazargronon.RCTZeroconf.nsd.NsdServiceImpl;
import com.balthazargronon.RCTZeroconf.rx2dnssd.DnssdImpl;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;

import org.apache.commons.lang3.StringUtils;

import java.util.HashMap;
import java.util.Map;

public class ZeroConfImplFactory {
    public static final String NSD_IMPL = "NSD";
    public static final String DNSSD_IMPL = "DNSSD";

    private Map<String, Zeroconf> zeroconfMap = new HashMap<>();

    private ZeroconfModule zeroconfModule;
    private ReactApplicationContext context;

    public ZeroConfImplFactory(ZeroconfModule zeroconfModule, ReactApplicationContext context) {
        this.zeroconfModule = zeroconfModule;
        this.context = context;
    }

    public Zeroconf getZeroconf(String implType) {
        if (StringUtils.isBlank(implType)) implType = ZeroConfImplFactory.NSD_IMPL;
        return getOrCreateImpl(implType);
    }

    private Zeroconf getOrCreateImpl(String implType) {
        if (!zeroconfMap.containsKey(implType)) {
            switch (implType) {
                case NSD_IMPL:
                    zeroconfMap.put(NSD_IMPL, new NsdServiceImpl(zeroconfModule, context));
                    break;
                case DNSSD_IMPL:
                    zeroconfMap.put(DNSSD_IMPL, new DnssdImpl(zeroconfModule, context));
                    break;
                default:
                    throw new IllegalArgumentException(String.format("%s implType is not supported. Only %s and %s are supported", implType, NSD_IMPL, DNSSD_IMPL));
            }
        }

        return zeroconfMap.get(implType);
    }

}
