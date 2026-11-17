/**
 * Permanent information about a player (Phase 2 class diagram).
 * The connection time is kept here because it doesn't belong to one match.
 */
public class Player {

	private final int id;
	private final String name;
	private final String xbeeLabel;
	private volatile long lastSeen = 0; // last time a message arrived from this gun

	public Player(int id, String name, String xbeeLabel) {
		this.id = id;
		this.name = name;
		this.xbeeLabel = xbeeLabel;
	}

	public int getId() {
		return id;
	}

	public String getName() {
		return name;
	}

	public String getXbeeLabel() {
		return xbeeLabel;
	}

	public void markSeen() {
		lastSeen = System.currentTimeMillis();
	}

	/** connected = a message arrived in the last 3 seconds (guns send one every second) */
	public boolean isConnected() {
		return System.currentTimeMillis() - lastSeen < 3000;
	}
}
