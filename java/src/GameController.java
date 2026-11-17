import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * Main control class of the Java program (Phase 2 class diagram).
 *
 * OBSERVER pattern: GameController is the ConcreteSubject (it implements
 * Subject). It keeps the list of observers, lets them register and leave,
 * and calls update(...) on all of them after every change. This is the
 * same structure as the lecture's ConcreteSubject / Clock.
 *
 * SINGLETON pattern: there is exactly one game controller, reached with
 * GameController.getInstance().
 *
 * THREADS:
 *   - "XBee reader"  (in XBeeLink) puts events into the queue below
 *   - "Game logic"   takes events out of the queue one by one and applies
 *                    them to the match (producer / consumer)
 *   - "Watchdog"     every second checks which guns are still sending
 *   - Swing thread   draws the dashboard (Dashboard uses invokeLater)
 */
public final class GameController implements Subject {

	private static GameController instance;

	public static synchronized GameController getInstance() {
		if (instance == null) instance = new GameController();
		return instance;
	}

	// ---------- settings ----------
	public static final int DEFAULT_HEALTH = 10;
	public static final int MAGAZINE_SIZE = 6;   // same as the gun code
	private final int damagePerHit = 1;
	private volatile int maxHealth = DEFAULT_HEALTH;

	private final List<Player> players = Arrays.asList(
			new Player(1, "Player 1", "22 TX1"),
			new Player(2, "Player 2", "22 TX2"));

	private final BlockingQueue<GameEvent> events = new LinkedBlockingQueue<GameEvent>();
	private final List<Observer> observers = new ArrayList<Observer>();
	private final boolean[] wasConnected = new boolean[3];
	private volatile Match match = new Match(players, maxHealth, damagePerHit);
	private volatile boolean showRawLines = false;
	private XBeeLink link;

	private GameController() {
	}

	// ---------- Subject (Observer pattern) ----------
	@Override
	public void registerObserver(Observer o) {
		synchronized (observers) {
			if (!observers.contains(o)) observers.add(o);
		}
	}

	@Override
	public void removeObserver(Observer o) {
		synchronized (observers) {
			observers.remove(o);
		}
	}

	@Override
	public void notifyObservers(String message) {
		Observer[] copy;
		synchronized (observers) {          // copy, so an observer may leave during the loop
			copy = observers.toArray(new Observer[0]);
		}
		for (Observer o : copy) {
			o.update(match, message);
		}
	}

	// ---------- setup ----------
	public void setLink(XBeeLink link) {
		this.link = link;
	}

	/** starts the game logic thread and the watchdog */
	public void start() {
		Thread game = new Thread(new Runnable() {
			public void run() {
				gameLoop();
			}
		}, "Game logic");
		game.setDaemon(true);
		game.start();

		ScheduledExecutorService watchdog = Executors.newSingleThreadScheduledExecutor();
		watchdog.scheduleAtFixedRate(new Runnable() {
			public void run() {
				checkConnections();
			}
		}, 1, 1, TimeUnit.SECONDS);

		notifyObservers("Controller started. Waiting for the guns...");
	}

	// ---------- called by the XBee reader thread ----------
	public void submit(GameEvent event) {
		events.offer(event);
	}

	public void rawLine(String line) {
		if (showRawLines) notifyObservers("RAW: " + line);
	}

	public void log(String message) {
		notifyObservers(message);
	}

	// ---------- game logic thread ----------
	private void gameLoop() {
		while (true) {
			try {
				handleEvent(events.take()); // waits until an event arrives
			} catch (InterruptedException e) {
				return;
			}
		}
	}

	private synchronized void handleEvent(GameEvent event) {
		Player player = match.getPlayer(event.getPlayerId());
		if (player == null) return;
		player.markSeen();

		boolean wasRunning = match.isRunning();
		boolean counted = match.handle(event);

		String message = null;
		if (event instanceof StatusEvent) {
			if (((StatusEvent) event).isJoined()) message = event.describe();
		} else {
			message = counted ? event.describe() : event.describe() + "  (not counted)";
		}
		notifyObservers(message);

		if (wasRunning && match.isOver()) {
			int winner = match.getWinnerId();
			notifyObservers("*** GAME OVER: " + match.getPlayer(winner).getName() + " wins! ***");
			send("<W," + winner + ">");
		}
	}

	// ---------- called by the dashboard (Game Host) ----------
	public synchronized void startMatch(int health) {
		maxHealth = health;
		match = new Match(players, maxHealth, damagePerHit);
		match.start();
		send("<S," + health + ">");
		notifyObservers("Match #" + match.getMatchId() + " started, " + health
				+ " health each, " + MAGAZINE_SIZE + " blasts per magazine. Start command sent to the guns.");
	}

	public void setShowRawLines(boolean show) {
		showRawLines = show;
	}

	public Match getMatch() {
		return match;
	}

	public List<Player> getPlayers() {
		return players;
	}

	public int getMaxHealth() {
		return maxHealth;
	}

	// ---------- helpers ----------
	private void send(String command) {
		if (link != null) link.send(command);
	}

	private void checkConnections() {
		for (Player p : players) {
			boolean now = p.isConnected();
			if (now != wasConnected[p.getId()]) {
				wasConnected[p.getId()] = now;
				notifyObservers(p.getName() + (now ? " gun is sending" : " gun LOST (no message for 3 s)"));
			}
		}
		notifyObservers(null); // refresh the screen (match timer)
	}
}
