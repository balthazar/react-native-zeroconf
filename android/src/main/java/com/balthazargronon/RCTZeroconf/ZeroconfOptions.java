package com.balthazargronon.RCTZeroconf;


import java.util.ArrayList;
import java.util.List;

import javax.annotation.Nullable;

/**
 * Options of scan, registerService and resolveService
 */
public class ZeroconfOptions {
    /** Scan: subtype to browse, "_printer" */
    @Nullable public String subtype;
    /** Publish: subtypes to register, "_printer" */
    public final List<String> subtypes = new ArrayList<>();
    /** Network interface name, "wlan0" */
    @Nullable public String networkInterface;
    /** resolveService: seconds before giving up */
    public double timeoutSeconds = 5;
    /** Scan: seconds to resolve each service, retried once before a TIMEOUT error */
    public double resolveTimeoutSeconds = 5;

    /**
     * @param timeout resolveService's timeout in seconds, ignored unless positive
     * @param resolveTimeout scans' resolve timeout in seconds, ignored unless positive
     */
    public ZeroconfOptions(@Nullable String subtype, @Nullable String[] subtypes, @Nullable String networkInterface, double timeout, double resolveTimeout) {
        this.subtype = subtypeLabel(subtype);
        if (subtypes != null) {
            for (String candidate : subtypes) {
                String label = subtypeLabel(candidate);
                if (label != null) {
                    this.subtypes.add(label);
                }
            }
        }
        this.networkInterface = networkInterface == null || networkInterface.isEmpty() ? null : networkInterface;
        if (timeout > 0) {
            this.timeoutSeconds = timeout;
        }
        if (resolveTimeout > 0) {
            this.resolveTimeoutSeconds = resolveTimeout;
        }
    }

    /**
     * "_ipp._tcp" with the subtypes: "_ipp._tcp,_printer", the syntax of mDNSResponder and NsdManager
     */
    public static String withSubtypes(String serviceType, List<String> subtypes) {
        StringBuilder type = new StringBuilder(serviceType);
        for (String subtype : subtypes) {
            type.append(',').append(subtype);
        }
        return type.toString();
    }

    // "printer" or "_printer" -> "_printer"
    @Nullable
    private static String subtypeLabel(@Nullable String subtype) {
        if (subtype == null || subtype.isEmpty()) {
            return null;
        }
        return subtype.startsWith("_") ? subtype : "_" + subtype;
    }
}
