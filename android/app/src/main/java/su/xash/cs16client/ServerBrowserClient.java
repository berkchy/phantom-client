package su.xash.cs16client;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.HashMap;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadLocalRandom;

public class ServerBrowserClient {
    public interface DebugSink {
        void log(String line);
    }

    private static final String TAG = "CS16ServerBrowser";
    private static final int MASTER_PORT = 27010;
    private static final int SERVER_PORT = 27015;
    private static final int MASTER_TIMEOUT_MS = 1400;
    private static final int SERVER_TIMEOUT_MS = 1100;
    private static final int MAX_SERVER_RESULTS = 64;

    private static final String[] MASTER_HOSTS = new String[] {
            "mentality.rip:27010",
            "ms2.mentality.rip:27010",
            "ms3.mentality.rip:27010",
            "mentality.rip:27011",
    };

    private final DebugSink sink;

    public ServerBrowserClient() {
        this(null);
    }

    public ServerBrowserClient(DebugSink sink) {
        this.sink = sink;
    }

    private void emit(String line) {
        String value = line == null ? "" : line;
        Log.d(TAG, value);
        if (sink != null) {
            sink.log(value);
        }
    }

    public List<ServerEntry> refresh(String gameDir) {
        emit("refresh: start gameDir=" + gameDir);
        int scanKey = ThreadLocalRandom.current().nextInt(1, Integer.MAX_VALUE);
        emit("refresh: scanKey=" + Integer.toHexString(scanKey));

        Map<String, ServerEntry> servers = new LinkedHashMap<>();
        List<InetSocketAddress> masters = resolveMasters();
        emit("refresh: resolved masters=" + masters.size() + " " + masters);
        if (masters.isEmpty()) {
            emit("refresh: no masters resolved");
            return new ArrayList<>();
        }

        byte[] currentRequest = buildMasterRequest(gameDir, scanKey, true, true);
        byte[] goldsrcRequest = buildMasterRequest(gameDir, 0, false, false);
        emit("refresh: current master request len=" + currentRequest.length + " hex=" + toHex(currentRequest, currentRequest.length));
        emit("refresh: goldsrc master request len=" + goldsrcRequest.length + " hex=" + toHex(goldsrcRequest, goldsrcRequest.length));

        for (InetSocketAddress master : masters) {
            emit("refresh: querying master(current) " + formatAddress(master));
            servers.putAll(queryMaster(master, currentRequest, scanKey, "current"));
            emit("refresh: master aggregate size=" + servers.size());

            emit("refresh: querying master(goldsrc) " + formatAddress(master));
            servers.putAll(queryMaster(master, goldsrcRequest, 0, "goldsrc"));
            emit("refresh: master aggregate size=" + servers.size());
        }

        List<ServerEntry> result = new ArrayList<>(servers.values());
        if (result.size() > MAX_SERVER_RESULTS) {
            emit("refresh: truncating server candidates " + result.size() + " -> " + MAX_SERVER_RESULTS);
            result = result.subList(0, MAX_SERVER_RESULTS);
        }

        emit("refresh: candidate servers=" + result.size());

        ExecutorService pool = Executors.newFixedThreadPool(8);
        try {
            List<Future<ServerEntry>> futures = new ArrayList<>();
            for (ServerEntry entry : result) {
                emit("refresh: scheduling server query " + entry.address);
                futures.add(pool.submit(new ServerInfoTask(entry.address)));
            }

            List<ServerEntry> finalList = new ArrayList<>();
            for (Future<ServerEntry> future : futures) {
                try {
                    ServerEntry entry = future.get();
                    if (entry != null) {
                        finalList.add(entry);
                        emit("refresh: server ok address=" + entry.address
                                + " name=" + safe(entry.name)
                                + " map=" + safe(entry.map)
                                + " players=" + entry.players + "/" + entry.maxPlayers
                                + " ping=" + entry.pingMs);
                    } else {
                        emit("refresh: server query returned null");
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    emit("refresh: interrupted while waiting for queries");
                    break;
                } catch (ExecutionException e) {
                    emit("refresh: query failed " + e);
                }
            }

            finalList.sort((a, b) -> {
                int cmp = Integer.compare(a.pingMs, b.pingMs);
                if (cmp != 0) return cmp;
                return a.name.compareToIgnoreCase(b.name);
            });
            emit("refresh: final list size=" + finalList.size());
            return finalList;
        } finally {
            emit("refresh: shutting down pool");
            pool.shutdownNow();
        }
    }

    public ServerEntry queryDirect(String address) {
        return new ServerInfoTask(normalizeAddress(address)).callQuietly();
    }

    private List<InetSocketAddress> resolveMasters() {
        Set<String> seen = new LinkedHashSet<>();
        List<InetSocketAddress> out = new ArrayList<>();
        for (String host : MASTER_HOSTS) {
            if (!seen.add(host)) {
                continue;
            }
            InetSocketAddress adr = parseAddress(host, MASTER_PORT);
            if (adr != null) {
                out.add(adr);
            }
        }
        return out;
    }

    private Map<String, ServerEntry> queryMaster(InetSocketAddress master, byte[] request, int scanKey, String label) {
        Map<String, ServerEntry> servers = new LinkedHashMap<>();
        try (DatagramSocket socket = new DatagramSocket()) {
            socket.setSoTimeout(MASTER_TIMEOUT_MS);
            emit("master[" + label + "]: send len=" + request.length + " to " + formatAddress(master));
            socket.send(new DatagramPacket(request, request.length, master));

            long deadline = System.currentTimeMillis() + MASTER_TIMEOUT_MS;
            byte[] buf = new byte[4096];
            while (System.currentTimeMillis() < deadline) {
                DatagramPacket packet = new DatagramPacket(buf, buf.length);
                try {
                    socket.receive(packet);
                } catch (SocketTimeoutException e) {
                    emit("master[" + label + "]: timeout from " + formatAddress(master));
                    break;
                }

                emit("master[" + label + "]: packet len=" + packet.getLength() + " from "
                        + packet.getAddress().getHostAddress() + ":" + packet.getPort()
                        + " hex=" + toHex(packet.getData(), packet.getLength()));
                for (InetSocketAddress adr : parseMasterResponse(packet.getData(), packet.getLength(), scanKey)) {
                    String key = adr.getAddress().getHostAddress() + ":" + adr.getPort();
                    if (key.equals(formatAddress(master))) {
                        emit("master[" + label + "]: skipping self entry " + key);
                        continue;
                    }
                    if (!servers.containsKey(key)) {
                        servers.put(key, new ServerEntry(key));
                        emit("master[" + label + "]: discovered server " + key);
                    }
                }
            }
        } catch (Exception e) {
            emit("master[" + label + "]: query failed for " + formatAddress(master) + " " + e);
        }
        return servers;
    }

    private byte[] buildMasterRequest(String gameDir, int scanKey, boolean includeClientInfo, boolean includeKey) {
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            out.write(new byte[] { '1', (byte) 0xFF });
            out.write("0.0.0.0:0".getBytes(StandardCharsets.US_ASCII));
            out.write(0);

            StringBuilder info = new StringBuilder();
            appendInfoField(info, "gamedir", gameDir);
            if (includeClientInfo) {
                appendInfoField(info, "clver", "android");
                appendInfoField(info, "nat", "0");
                appendInfoField(info, "commit", "android");
                appendInfoField(info, "branch", "android");
                appendInfoField(info, "os", "android");
                appendInfoField(info, "arch", android.os.Build.SUPPORTED_ABIS.length > 0 ? android.os.Build.SUPPORTED_ABIS[0] : "arm64");
                appendInfoField(info, "buildnum", "1");
            }
            if (includeKey) {
                appendInfoField(info, "key", Integer.toHexString(scanKey));
            }

            out.write(info.toString().getBytes(StandardCharsets.US_ASCII));
            return out.toByteArray();
        } catch (Exception e) {
            emit("refresh: failed to build master request " + e);
            return new byte[0];
        }
    }

    private List<InetSocketAddress> parseMasterResponse(byte[] data, int length, int scanKey) {
        List<InetSocketAddress> out = new ArrayList<>();
        if (length < 5) {
            emit("master: response too short len=" + length);
            return out;
        }

        ByteBuffer buf = ByteBuffer.wrap(data, 0, length).order(ByteOrder.BIG_ENDIAN);
        int header = buf.getInt();
        if (header != -1) {
            emit("master: invalid header=" + header);
            return out;
        }

        String token = readToken(data, 4, length);
        emit("master: token=\"" + token + "\"");
        if (!"f".equals(token)) {
            emit("master: ignoring token=" + token);
            return out;
        }

        int pos = 4 + token.length();
        if (pos < length && (data[pos] & 0xFF) == 0x0A) {
            pos++;
        }

        if (pos < length && (data[pos] & 0xFF) == 0x7F) {
            pos++;
            if (pos + 5 > length) {
                emit("master: short key header");
                return out;
            }

            int key = ((data[pos++] & 0xFF) << 24)
                    | ((data[pos++] & 0xFF) << 16)
                    | ((data[pos++] & 0xFF) << 8)
                    | (data[pos++] & 0xFF);
            if (scanKey != 0 && key != scanKey) {
                emit("master: invalid key " + Integer.toHexString(key) + " expected=" + Integer.toHexString(scanKey));
                return out;
            }
            pos++; // reserved byte
        } else if (pos < length && (data[pos] & 0xFF) == 0xFF) {
            pos++;
        }

        while (pos + 6 <= length) {
            int a = data[pos++] & 0xFF;
            int b = data[pos++] & 0xFF;
            int c = data[pos++] & 0xFF;
            int d = data[pos++] & 0xFF;
            int port = ((data[pos++] & 0xFF) << 8) | (data[pos++] & 0xFF);
            if (a == 0 && b == 0 && c == 0 && d == 0 && port == 0) {
                emit("master: terminator reached");
                break;
            }
            if (port == 0) {
                emit("master: port zero terminator reached");
                break;
            }

            InetSocketAddress adr = new InetSocketAddress(a + "." + b + "." + c + "." + d, port);
            out.add(adr);
            emit("master: parsed address " + formatAddress(adr));
        }

        return out;
    }

    private void appendInfoField(StringBuilder info, String key, String value) {
        if (key == null || key.isEmpty()) {
            return;
        }
        info.append('\\').append(key).append('\\').append(value == null ? "" : value);
    }

    private final class ServerInfoTask implements Callable<ServerEntry> {
        private final String address;

        private ServerInfoTask(String address) {
            this.address = normalizeAddress(address);
        }

        @Override
        public ServerEntry call() {
            if (address.isEmpty()) {
                emit("server: empty address");
                return null;
            }

            InetSocketAddress adr = parseAddress(address, SERVER_PORT);
            if (adr == null) {
                emit("server: invalid address=" + address);
                return null;
            }

            long started = System.nanoTime();
            ServerEntry entry = queryCurrentInfo(address, adr);
            if (entry == null) {
                emit("server: current query failed, falling back to goldsrc for " + address);
                entry = queryGoldSrcInfo(address, adr);
            }

            if (entry != null) {
                entry.pingMs = (int) Math.max(1L, (System.nanoTime() - started) / 1_000_000L);
                emit("server: ok address=" + address
                        + " name=" + safe(entry.name)
                        + " map=" + safe(entry.map)
                        + " gameDir=" + safe(entry.gameDir)
                        + " version=" + safe(entry.version)
                        + " players=" + entry.players + "/" + entry.maxPlayers
                        + " bots=" + entry.bots
                        + " password=" + entry.password
                        + " ping=" + entry.pingMs);
            }
            return entry;
        }

        private ServerEntry callQuietly() {
            try {
                return call();
            } catch (Exception e) {
                return null;
            }
        }

        private ServerEntry queryCurrentInfo(String address, InetSocketAddress adr) {
            byte[] request = buildCurrentInfoQuery();
            emit("server[current]: send len=" + request.length + " to " + address + " resolved=" + formatAddress(adr) + " hex=" + toHex(request, request.length));
            byte[] buf = new byte[4096];

            try (DatagramSocket socket = new DatagramSocket()) {
                socket.setSoTimeout(SERVER_TIMEOUT_MS);
                socket.send(new DatagramPacket(request, request.length, adr));
                emit("server[current]: waiting response from " + address);

                DatagramPacket packet = new DatagramPacket(buf, buf.length);
                socket.receive(packet);
                emit("server[current]: packet len=" + packet.getLength() + " from " + packet.getAddress().getHostAddress() + ":" + packet.getPort() + " hex=" + toHex(packet.getData(), packet.getLength()));

                ServerEntry entry = parseCurrentInfoResponse(address, packet.getData(), packet.getLength());
                if (entry == null) {
                    emit("server[current]: parse returned null for " + address);
                }
                return entry;
            } catch (SocketTimeoutException e) {
                emit("server[current]: timeout for " + address);
                return null;
            } catch (Exception e) {
                emit("server[current]: query failed for " + address + " " + e);
                return null;
            }
        }

        private ServerEntry queryGoldSrcInfo(String address, InetSocketAddress adr) {
            byte[] request = buildGoldSrcInfoQuery();
            emit("server[goldsrc]: send len=" + request.length + " to " + address + " resolved=" + formatAddress(adr) + " hex=" + toHex(request, request.length));
            byte[] buf = new byte[4096];

            try (DatagramSocket socket = new DatagramSocket()) {
                socket.setSoTimeout(SERVER_TIMEOUT_MS);
                socket.send(new DatagramPacket(request, request.length, adr));
                emit("server[goldsrc]: waiting response from " + address);

                DatagramPacket packet = new DatagramPacket(buf, buf.length);
                socket.receive(packet);
                emit("server[goldsrc]: packet len=" + packet.getLength() + " from " + packet.getAddress().getHostAddress() + ":" + packet.getPort() + " hex=" + toHex(packet.getData(), packet.getLength()));

                ServerEntry entry = parseGoldSrcResponse(address, packet.getData(), packet.getLength());
                if (entry == null) {
                    emit("server[goldsrc]: parse returned null for " + address);
                }
                return entry;
            } catch (SocketTimeoutException e) {
                emit("server[goldsrc]: timeout for " + address);
                return null;
            } catch (Exception e) {
                emit("server[goldsrc]: query failed for " + address + " " + e);
                return null;
            }
        }
    }

    private byte[] buildCurrentInfoQuery() {
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            out.write(new byte[] { (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF });
            out.write("info 49".getBytes(StandardCharsets.US_ASCII));
            return out.toByteArray();
        } catch (Exception e) {
            return new byte[0];
        }
    }

    private byte[] buildGoldSrcInfoQuery() {
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            out.write(new byte[] { (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF });
            out.write("TSource Engine Query".getBytes(StandardCharsets.US_ASCII));
            out.write(0);
            return out.toByteArray();
        } catch (Exception e) {
            return new byte[0];
        }
    }

    private ServerEntry parseCurrentInfoResponse(String address, byte[] data, int length) {
        if (length < 6) {
            emit("server[current]: response too short address=" + address + " len=" + length);
            return null;
        }

        ByteBuffer buf = ByteBuffer.wrap(data, 0, length).order(ByteOrder.BIG_ENDIAN);
        int header = buf.getInt();
        if (header != -1) {
            emit("server[current]: invalid header=" + header + " address=" + address);
            return null;
        }

        String token = readToken(data, 4, length);
        if (!"info".equals(token)) {
            emit("server[current]: unsupported token=\"" + token + "\" address=" + address);
            return null;
        }

        int pos = 4 + token.length();
        if (pos < length && data[pos] == '\n') {
            pos++;
        }

        String payload = new String(data, pos, length - pos, StandardCharsets.US_ASCII).trim();
        if (payload.isEmpty()) {
            emit("server[current]: empty payload address=" + address);
            return null;
        }

        Map<String, String> info = parseInfoString(payload);
        String host = firstNonEmpty(info.get("host"), info.get("hostname"), address);
        String map = firstNonEmpty(info.get("map"), "");
        String gameDir = firstNonEmpty(info.get("gamedir"), info.get("game"), "");
        String version = firstNonEmpty(info.get("p"), info.get("protocol"), "");
        int players = parseInt(firstNonEmpty(info.get("numcl"), info.get("current"), info.get("players")), 0);
        int maxPlayers = parseInt(firstNonEmpty(info.get("maxcl"), info.get("max")), 0);
        boolean password = "1".equals(firstNonEmpty(info.get("password"), "0"));

        ServerEntry entry = new ServerEntry(address);
        entry.name = stripColorCodes(host);
        entry.map = stripColorCodes(map);
        entry.gameDir = stripColorCodes(gameDir);
        entry.version = stripColorCodes(version);
        entry.players = players;
        entry.maxPlayers = maxPlayers;
        entry.bots = 0;
        entry.password = password;

        emit("server[current]: parsed address=" + address
                + " host=\"" + host + "\""
                + " map=\"" + map + "\""
                + " gameDir=\"" + gameDir + "\""
                + " version=\"" + version + "\""
                + " players=" + players + "/" + maxPlayers
                + " password=" + password);
        return entry;
    }

    private ServerEntry parseGoldSrcResponse(String address, byte[] data, int length) {
        if (length < 6) {
            emit("server[goldsrc]: response too short address=" + address + " len=" + length);
            return null;
        }

        ByteBuffer buf = ByteBuffer.wrap(data, 0, length).order(ByteOrder.BIG_ENDIAN);
        int header = buf.getInt();
        if (header != -1) {
            emit("server[goldsrc]: invalid header=" + header + " address=" + address);
            return null;
        }

        int type = buf.get() & 0xFF;
        if (type != 'I') {
            emit("server[goldsrc]: unsupported packet type=" + type + " address=" + address);
            return null;
        }

        if (!buf.hasRemaining()) {
            emit("server[goldsrc]: missing protocol byte address=" + address);
            return null;
        }

        buf.get(); // protocol byte
        String host = readCString(buf);
        String map = readCString(buf);
        String gameDir = readCString(buf);
        String gameDesc = readCString(buf);
        if (buf.remaining() < 2 + 6) {
            emit("server[goldsrc]: missing fields address=" + address
                    + " host=" + host + " map=" + map
                    + " gameDir=" + gameDir + " gameDesc=" + gameDesc
                    + " remaining=" + buf.remaining());
            return null;
        }

        buf.getShort(); // app id
        int players = buf.get() & 0xFF;
        int maxPlayers = buf.get() & 0xFF;
        int bots = buf.get() & 0xFF;
        buf.get(); // dedicated
        buf.get(); // os
        boolean password = (buf.get() & 0xFF) != 0;
        String version = buf.hasRemaining() ? readCString(buf) : "";

        ServerEntry entry = new ServerEntry(address);
        entry.name = stripColorCodes(host);
        entry.map = stripColorCodes(map);
        entry.version = stripColorCodes(version.isEmpty() ? gameDesc : version);
        entry.gameDir = stripColorCodes(gameDir);
        entry.players = players;
        entry.maxPlayers = maxPlayers;
        entry.bots = bots;
        entry.password = password;

        emit("server[goldsrc]: parsed address=" + address
                + " host=\"" + host + "\""
                + " map=\"" + map + "\""
                + " gameDir=\"" + gameDir + "\""
                + " gameDesc=\"" + gameDesc + "\""
                + " version=\"" + version + "\""
                + " players=" + players + "/" + maxPlayers
                + " bots=" + bots
                + " password=" + password);
        return entry;
    }

    private Map<String, String> parseInfoString(String payload) {
        Map<String, String> out = new HashMap<>();
        if (payload == null || payload.isEmpty()) {
            return out;
        }

        String text = payload;
        if (text.startsWith("info\n")) {
            text = text.substring(5);
        }

        int i = 0;
        while (i < text.length()) {
            while (i < text.length() && text.charAt(i) != '\\') {
                i++;
            }
            if (i >= text.length()) {
                break;
            }
            i++;
            int keyStart = i;
            while (i < text.length()) {
                char ch = text.charAt(i);
                if (ch == '\\' || ch == '\n' || ch == '\r') {
                    break;
                }
                i++;
            }
            if (i >= text.length() || text.charAt(i) != '\\') {
                break;
            }
            String key = text.substring(keyStart, i).trim();
            i++;
            int valueStart = i;
            while (i < text.length()) {
                char ch = text.charAt(i);
                if (ch == '\\' || ch == '\n' || ch == '\r') {
                    break;
                }
                i++;
            }
            String value = text.substring(valueStart, i).trim();
            if (!key.isEmpty()) {
                out.put(key, value);
            }
        }
        return out;
    }

    private String readToken(byte[] data, int offset, int length) {
        int end = offset;
        while (end < length) {
            byte b = data[end];
            if (b == 0 || b == '\n' || b == '\r' || b == ' ' || b == '\t') {
                break;
            }
            end++;
        }
        if (end <= offset) {
            return "";
        }
        return new String(data, offset, end - offset, StandardCharsets.US_ASCII);
    }

    private String readCString(ByteBuffer buf) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        while (buf.hasRemaining()) {
            byte b = buf.get();
            if (b == 0) {
                break;
            }
            out.write(b);
        }
        return new String(out.toByteArray(), StandardCharsets.UTF_8).trim();
    }

    private static String normalizeAddress(String address) {
        String value = address == null ? "" : address.trim();
        if (value.isEmpty()) {
            return "";
        }
        if (!value.contains(":")) {
            value = value + ":" + SERVER_PORT;
        }
        return value;
    }

    private InetSocketAddress parseAddress(String address, int defaultPort) {
        String value = address == null ? "" : address.trim();
        if (value.isEmpty()) {
            return null;
        }
        String host = value;
        int port = defaultPort;
        int colon = value.lastIndexOf(':');
        if (colon > 0 && colon < value.length() - 1) {
            host = value.substring(0, colon);
            try {
                port = Integer.parseInt(value.substring(colon + 1));
            } catch (NumberFormatException ignored) {
                port = defaultPort;
            }
        }

        try {
            return new InetSocketAddress(InetAddress.getByName(host), port);
        } catch (Exception e) {
            return null;
        }
    }

    private String firstNonEmpty(String... values) {
        if (values == null) {
            return "";
        }
        for (String value : values) {
            if (value != null && !value.trim().isEmpty()) {
                return value.trim();
            }
        }
        return "";
    }

    private int parseInt(String value, int fallback) {
        if (value == null || value.trim().isEmpty()) {
            return fallback;
        }
        try {
            return Integer.parseInt(value.trim());
        } catch (NumberFormatException e) {
            return fallback;
        }
    }

    private String stripColorCodes(String input) {
        if (input == null || input.isEmpty()) {
            return "";
        }

        StringBuilder out = new StringBuilder(input.length());
        for (int i = 0; i < input.length(); i++) {
            char ch = input.charAt(i);
            if (ch == '^' && i + 1 < input.length() && Character.isDigit(input.charAt(i + 1))) {
                i++;
                continue;
            }
            out.append(ch);
        }
        return out.toString().trim();
    }

    private String safe(String value) {
        return value == null ? "" : value;
    }

    private String formatAddress(InetSocketAddress address) {
        if (address == null || address.getAddress() == null) {
            return String.valueOf(address);
        }
        return address.getAddress().getHostAddress() + ":" + address.getPort();
    }

    private String toHex(byte[] data, int length) {
        if (data == null || length <= 0) {
            return "";
        }
        int limit = Math.min(length, 64);
        StringBuilder sb = new StringBuilder(limit * 3);
        for (int i = 0; i < limit; i++) {
            if (i > 0) {
                sb.append(' ');
            }
            sb.append(String.format(Locale.US, "%02X", data[i] & 0xFF));
        }
        if (length > limit) {
            sb.append(" ...");
        }
        return sb.toString();
    }
}
