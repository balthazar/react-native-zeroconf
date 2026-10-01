package com.balthazargronon.RCTZeroconf;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.ReadableType;

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

    public static ZeroconfOptions from(@Nullable ReadableMap map) {
        ZeroconfOptions options = new ZeroconfOptions();
        if (map == null) {
            return options;
        }
        options.subtype = subtypeLabel(getString(map, "subtype"));
        if (map.hasKey("subtypes") && map.getType("subtypes") == ReadableType.Array) {
            ReadableArray subtypes = map.getArray("subtypes");
            for (int i = 0; i < subtypes.size(); i++) {
                String label = subtypes.getType(i) == ReadableType.String ? subtypeLabel(subtypes.getString(i)) : null;
                if (label != null) {
                    options.subtypes.add(label);
                }
            }
        }
        String networkInterface = getString(map, "networkInterface");
        options.networkInterface = networkInterface == null || networkInterface.isEmpty() ? null : networkInterface;
        if (map.hasKey("timeout") && map.getType("timeout") == ReadableType.Number && map.getDouble("timeout") > 0) {
            options.timeoutSeconds = map.getDouble("timeout");
        }
        return options;
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

    @Nullable
    private static String getString(ReadableMap map, String key) {
        return map.hasKey(key) && map.getType(key) == ReadableType.String ? map.getString(key) : null;
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
