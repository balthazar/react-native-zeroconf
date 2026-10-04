package com.balthazargronon.RCTZeroconf;

import com.facebook.proguard.annotations.DoNotStrip;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.Nullable;

/**
 * An event or result of the NSD implementation, read by the C++ module (cpp/android/NsdBackend) through JNI:
 * a service's name, full name, host, port, addresses and TXT record.
 */
@DoNotStrip
public final class NsdPayload {
    private final Map<String, Object> values = new LinkedHashMap<>();

    public void putString(String key, @Nullable String value) {
        values.put(key, value);
    }

    public void putInt(String key, int value) {
        values.put(key, value);
    }

    public void putArray(String key, List<String> value) {
        values.put(key, new ArrayList<>(value));
    }

    public void putMap(String key, NsdPayload value) {
        values.put(key, value);
    }

    @DoNotStrip
    @Nullable
    public String getString(String key) {
        Object value = values.get(key);
        return value instanceof String ? (String) value : null;
    }

    @DoNotStrip
    public int getInt(String key) {
        Object value = values.get(key);
        return value instanceof Integer ? (Integer) value : 0;
    }

    @DoNotStrip
    public String[] getStrings(String key) {
        Object value = values.get(key);
        if (!(value instanceof List)) {
            return new String[0];
        }
        List<?> list = (List<?>) value;
        String[] strings = new String[list.size()];
        for (int i = 0; i < strings.length; i++) {
            strings[i] = String.valueOf(list.get(i));
        }
        return strings;
    }

    // The keys, then the values, of a nested map of strings (the TXT record), in order
    @DoNotStrip
    public String[] getMapKeys(String key) {
        Object value = values.get(key);
        if (!(value instanceof NsdPayload)) {
            return new String[0];
        }
        return ((NsdPayload) value).values.keySet().toArray(new String[0]);
    }

    @DoNotStrip
    public String[] getMapValues(String key) {
        Object value = values.get(key);
        if (!(value instanceof NsdPayload)) {
            return new String[0];
        }
        Map<String, Object> map = ((NsdPayload) value).values;
        String[] strings = new String[map.size()];
        int i = 0;
        for (Object entry : map.values()) {
            strings[i++] = entry == null ? "" : String.valueOf(entry);
        }
        return strings;
    }
}
