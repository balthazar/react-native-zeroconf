package com.balthazargronon.RCTZeroconf.nsd;

import android.text.TextUtils;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.MulticastSocket;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

import javax.annotation.Nullable;

/**
 * Lists the service types on the network by querying the PTR records of _services._dns-sd._udp.local.
 * NsdManager can't: on Android 14 and later it reads that name as a subtype of _dns-sd._udp.
 *
 * Queries are sent from an ephemeral port (RFC 6762 legacy unicast), so responders answer directly to
 * this socket and the system's mDNS port 5353 is left alone. Types are queried again periodically, and
 * reported lost when they stop being answered.
 */
class ServiceTypesQuery {
    private static final String TAG = "ServiceTypesQuery";
    private static final String QUERY_NAME = "_services._dns-sd._udp.local";
    private static final int TYPE_PTR = 12;
    private static final int CLASS_IN = 1;
    private static final int MDNS_PORT = 5353;
    // Query again after 1, 2 and 4 seconds, then every 10 seconds
    private static final long[] QUERY_DELAYS_MS = { 1000, 1000, 2000 };
    private static final long QUERY_INTERVAL_MS = 10000;
    // Lost after three unanswered queries
    private static final long EXPIRY_MS = 3 * QUERY_INTERVAL_MS + 5000;

    interface Listener {
        void onTypeFound(String type);

        void onTypeLost(String type);

        void onError(String message);
    }

    private final Listener listener;
    private final Map<String, Long> lastSeen = new HashMap<>();
    private final int queryId = 1 + new Random().nextInt(0xfffe);
    private volatile boolean running;
    @Nullable private MulticastSocket socket;
    @Nullable private Thread thread;

    ServiceTypesQuery(Listener listener) {
        this.listener = listener;
    }

    void start() {
        running = true;
        thread = new Thread(this::run, "ServiceTypesQuery");
        thread.start();
    }

    void stop() {
        running = false;
        MulticastSocket current = socket;
        if (current != null) {
            current.close();
        }
        if (thread != null) {
            thread.interrupt();
        }
    }

    private void run() {
        try {
            NetworkInterface networkInterface = findInterface();
            if (networkInterface == null) {
                listener.onError("no network interface with IPv4 multicast");
                return;
            }
            InetAddress group = InetAddress.getByName("224.0.0.251");
            MulticastSocket multicastSocket = new MulticastSocket(0);
            multicastSocket.setNetworkInterface(networkInterface);
            multicastSocket.setTimeToLive(255);
            multicastSocket.setSoTimeout(500);
            socket = multicastSocket;

            byte[] query = buildQuery();
            byte[] buffer = new byte[9000];
            int queries = 0;
            long nextQuery = 0;
            while (running) {
                long now = System.currentTimeMillis();
                if (now >= nextQuery) {
                    multicastSocket.send(new DatagramPacket(query, query.length, group, MDNS_PORT));
                    nextQuery = now + (queries < QUERY_DELAYS_MS.length ? QUERY_DELAYS_MS[queries] : QUERY_INTERVAL_MS);
                    queries++;
                    expire(now);
                }
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                try {
                    multicastSocket.receive(packet);
                } catch (SocketTimeoutException e) {
                    continue;
                }
                for (Map.Entry<String, Long> answer : parseAnswers(packet.getData(), packet.getLength()).entrySet()) {
                    onAnswer(answer.getKey(), answer.getValue(), System.currentTimeMillis());
                }
            }
        } catch (IOException e) {
            if (running) {
                Log.e(TAG, "Service types query failed", e);
                listener.onError(e.getMessage());
            }
        } finally {
            MulticastSocket current = socket;
            if (current != null) {
                current.close();
            }
        }
    }

    private void onAnswer(String type, long ttlSeconds, long now) {
        if (ttlSeconds == 0) {
            // Goodbye record
            if (lastSeen.remove(type) != null) {
                listener.onTypeLost(type);
            }
            return;
        }
        if (lastSeen.put(type, now) == null) {
            listener.onTypeFound(type);
        }
    }

    private void expire(long now) {
        for (String type : new ArrayList<>(lastSeen.keySet())) {
            if (now - lastSeen.get(type) > EXPIRY_MS) {
                lastSeen.remove(type);
                listener.onTypeLost(type);
            }
        }
    }

    // The Wi-Fi (or Ethernet) interface: up, multicast, not loopback, with an IPv4 address
    @Nullable
    private static NetworkInterface findInterface() throws SocketException {
        NetworkInterface fallback = null;
        for (NetworkInterface candidate : Collections.list(NetworkInterface.getNetworkInterfaces())) {
            if (!candidate.isUp() || candidate.isLoopback() || !candidate.supportsMulticast() || candidate.isPointToPoint()) {
                continue;
            }
            boolean hasIPv4 = false;
            for (InterfaceAddress address : candidate.getInterfaceAddresses()) {
                hasIPv4 |= address.getAddress() instanceof Inet4Address;
            }
            if (!hasIPv4) {
                continue;
            }
            if (candidate.getName().startsWith("wlan") || candidate.getName().startsWith("eth")) {
                return candidate;
            }
            if (fallback == null) {
                fallback = candidate;
            }
        }
        return fallback;
    }

    private byte[] buildQuery() {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        // Header: id, flags, 1 question, no records. Legacy unicast queries use a non-zero id
        writeShort(out, queryId);
        writeShort(out, 0);
        writeShort(out, 1);
        writeShort(out, 0);
        writeShort(out, 0);
        writeShort(out, 0);
        for (String label : QUERY_NAME.split("\\.")) {
            out.write(label.length());
            out.write(label.getBytes(), 0, label.length());
        }
        out.write(0);
        writeShort(out, TYPE_PTR);
        writeShort(out, CLASS_IN);
        return out.toByteArray();
    }

    private static void writeShort(ByteArrayOutputStream out, int value) {
        out.write((value >> 8) & 0xff);
        out.write(value & 0xff);
    }

    /**
     * Service types ("_http._tcp") and their TTL from the PTR records of _services._dns-sd._udp.local in a response
     */
    static Map<String, Long> parseAnswers(byte[] data, int length) {
        Map<String, Long> types = new HashMap<>();
        try {
            if (length < 12 || (data[2] & 0x80) == 0) {
                // Not a response
                return types;
            }
            int questions = readShort(data, 4);
            int records = readShort(data, 6) + readShort(data, 8) + readShort(data, 10);
            int offset = 12;
            for (int i = 0; i < questions; i++) {
                offset = skipName(data, offset, length) + 4;
            }
            for (int i = 0; i < records && offset < length; i++) {
                List<String> name = new ArrayList<>();
                offset = readName(data, offset, length, name);
                int type = readShort(data, offset);
                long ttl = ((long) readShort(data, offset + 4) << 16) | readShort(data, offset + 6);
                int dataLength = readShort(data, offset + 8);
                int dataOffset = offset + 10;
                offset = dataOffset + dataLength;
                if (type != TYPE_PTR || !QUERY_NAME.equals(TextUtils.join(".", name).toLowerCase(Locale.ROOT))) {
                    continue;
                }
                List<String> target = new ArrayList<>();
                readName(data, dataOffset, length, target);
                if (target.size() >= 2 && target.get(0).startsWith("_")
                        && (target.get(1).equalsIgnoreCase("_tcp") || target.get(1).equalsIgnoreCase("_udp"))) {
                    types.put(target.get(0) + "." + target.get(1).toLowerCase(Locale.ROOT), ttl);
                }
            }
        } catch (IndexOutOfBoundsException e) {
            // Truncated or malformed packet, keep what was parsed
        }
        return types;
    }

    private static int readShort(byte[] data, int offset) {
        if (offset + 1 >= data.length) {
            throw new IndexOutOfBoundsException();
        }
        return ((data[offset] & 0xff) << 8) | (data[offset + 1] & 0xff);
    }

    private static int skipName(byte[] data, int offset, int length) {
        while (offset < length) {
            int labelLength = data[offset] & 0xff;
            if (labelLength == 0) {
                return offset + 1;
            }
            if ((labelLength & 0xc0) == 0xc0) {
                return offset + 2;
            }
            offset += 1 + labelLength;
        }
        throw new IndexOutOfBoundsException();
    }

    // Reads a name with compression pointers into labels, returns the offset after the name
    private static int readName(byte[] data, int offset, int length, List<String> labels) {
        int end = -1;
        int jumps = 0;
        while (true) {
            if (offset >= length) {
                throw new IndexOutOfBoundsException();
            }
            int labelLength = data[offset] & 0xff;
            if (labelLength == 0) {
                return end >= 0 ? end : offset + 1;
            }
            if ((labelLength & 0xc0) == 0xc0) {
                if (end < 0) {
                    end = offset + 2;
                }
                if (++jumps > 20) {
                    throw new IndexOutOfBoundsException();
                }
                offset = ((labelLength & 0x3f) << 8) | (data[offset + 1] & 0xff);
                continue;
            }
            if (offset + 1 + labelLength > length) {
                throw new IndexOutOfBoundsException();
            }
            labels.add(new String(data, offset + 1, labelLength, java.nio.charset.StandardCharsets.UTF_8));
            offset += 1 + labelLength;
        }
    }
}
