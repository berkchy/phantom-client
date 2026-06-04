package su.xash.cs16client;

public class ServerEntry {
    public final String address;
    public String name = "";
    public String map = "";
    public String gameDir = "";
    public String version = "";
    public int players;
    public int maxPlayers;
    public int bots;
    public int pingMs;
    public boolean password;

    public ServerEntry(String address) {
        this.address = address;
    }

    public String playersText() {
        return players + "/" + maxPlayers;
    }

    public String detailsText() {
        StringBuilder sb = new StringBuilder();
        if (map != null && !map.isEmpty()) {
            sb.append(map);
        }
        if (version != null && !version.isEmpty()) {
            if (sb.length() > 0) sb.append("  ");
            sb.append(version);
        }
        if (password) {
            if (sb.length() > 0) sb.append("  ");
            sb.append("password");
        }
        return sb.toString();
    }
}
