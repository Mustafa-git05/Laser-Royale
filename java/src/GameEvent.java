/**
 * Parent class of every game event (Phase 2 class diagram).
 * An event remembers which player it came from and when it happened,
 * and each type of event changes the match in its own way (apply).
 */
public abstract class GameEvent {

	protected final int playerId;
	protected final long timestamp;

	protected GameEvent(int playerId) {
		this.playerId = playerId;
		this.timestamp = System.currentTimeMillis();
	}

	public int getPlayerId() {
		return playerId;
	}

	public long getTimestamp() {
		return timestamp;
	}

	/** true if this event changes the score (fire / hit). Status events don't. */
	public abstract boolean isScoring();

	/** changes the match statistics for this event */
	public abstract void apply(Match match);

	/** one line for the log, eg. "Player 1 fired" */
	public abstract String describe();
}
