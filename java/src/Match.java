import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * One Laser Royale game: the players, their statistics, the current state
 * and the winner. All public methods are synchronized because the game
 * thread changes the match while the Swing thread reads it for the screen.
 */
public class Match {

	private static int nextMatchId = 1;
	private static final long RESET_GRACE_MS = 2000;
	private final Map<Integer, Boolean> waitingForReset = new LinkedHashMap<Integer, Boolean>();

	private final int matchId;
	private final int damagePerHit;
	private final Map<Integer, Player> players = new LinkedHashMap<Integer, Player>();
	private final Map<Integer, MatchStats> stats = new LinkedHashMap<Integer, MatchStats>();
	private MatchState state = new WaitingState();
	private long startTime = 0;
	private int winnerId = 0;

	public Match(List<Player> playerList, int maxHealth, int damagePerHit) {
		this.matchId = nextMatchId++;
		this.damagePerHit = damagePerHit;
		for (Player p : playerList) {
			players.put(p.getId(), p);
			stats.put(p.getId(), new MatchStats(maxHealth));
		}
	}

	public synchronized void start() {
		startTime = System.currentTimeMillis();
		state = new RunningState();
		for (Integer id : stats.keySet()) {
			waitingForReset.put(id, Boolean.TRUE);
		}
	}

	/**
	 * Right after Start, a gun may still send a message with its OLD totals
	 * (it was already on its way). Shots and hits of that player are only
	 * counted once the gun reports its reset (its totals go back to 0), or
	 * after RESET_GRACE_MS if the start command never reached that gun.
	 *
	 * @return true if a shot / hit was counted in the score
	 */
	public synchronized boolean handle(GameEvent event) {
		int id = event.getPlayerId();
		if (!stats.containsKey(id)) return false;
		if (event instanceof StatusEvent && ((StatusEvent) event).isJoined()) {
			waitingForReset.put(id, Boolean.FALSE);
		}
		if (event.isScoring() && Boolean.TRUE.equals(waitingForReset.get(id))) {
			if (System.currentTimeMillis() - startTime < RESET_GRACE_MS) return false; // old news, ignore
			waitingForReset.put(id, Boolean.FALSE);
		}
		boolean counted = event.isScoring() && state instanceof RunningState;
		state.handle(this, event);
		return counted;
	}

	/** called by RunningState after each event */
	synchronized void checkForWinner() {
		for (Map.Entry<Integer, MatchStats> e : stats.entrySet()) {
			if (e.getValue().isEliminated()) {
				winnerId = EventFactory.opponentOf(e.getKey());
				state = new GameOverState();
				return;
			}
		}
	}

	public synchronized MatchStats getStats(int playerId) {
		return stats.get(playerId);
	}

	public synchronized Player getPlayer(int playerId) {
		return players.get(playerId);
	}

	public synchronized boolean isRunning() {
		return state instanceof RunningState;
	}

	public synchronized boolean isOver() {
		return state instanceof GameOverState;
	}

	public synchronized String getStateName() {
		return state.getName();
	}

	public synchronized int getWinnerId() {
		return winnerId;
	}

	/** seconds since the start (0 before the start) */
	public synchronized long getSecondsPlayed() {
		return startTime == 0 ? 0 : (System.currentTimeMillis() - startTime) / 1000;
	}

	public int getMatchId() {
		return matchId;
	}

	public int getDamagePerHit() {
		return damagePerHit;
	}
}
