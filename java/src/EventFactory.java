import java.util.ArrayList;
import java.util.List;

/**
 * FACTORY design pattern: turns a text line received over ZigBee into
 * GameEvent objects (FireEvent, HitEvent, ReloadEvent, StatusEvent).
 *
 * The guns send running totals:
 *   <id,shots,hitsTaken,health,ammo,mag,reloads,check>
 *   eg. <1,12,3,7,4,1,2,30>     mag = 1 magazine in, 0 magazine out
 *   check = id + shots + hitsTaken + health + ammo + mag + reloads
 *
 * The factory remembers the last totals of each gun and creates one event
 * for every new shot, hit and reload. Because they are totals, a lost
 * message is not a problem: the next message still has the right numbers.
 * Anything outside < > (like the junk character ZigBee adds) is ignored,
 * and messages with a wrong check number are thrown away.
 */
public class EventFactory {

	public static final int MAX_PLAYERS = 2;
	private static final int FIELDS = 8;

	private final int[] lastShots = new int[MAX_PLAYERS + 1];
	private final int[] lastHits = new int[MAX_PLAYERS + 1];
	private final int[] lastReloads = new int[MAX_PLAYERS + 1];
	private final boolean[] seen = new boolean[MAX_PLAYERS + 1];

	/** all the events in one received line (empty list if it is noise) */
	public List<GameEvent> createEvents(String line) {
		List<GameEvent> events = new ArrayList<GameEvent>();
		int from = 0;
		while (true) {
			int start = line.indexOf('<', from);
			if (start < 0) break;
			int end = line.indexOf('>', start + 1);
			if (end < 0) break;
			createFromMessage(line.substring(start + 1, end), events);
			from = end + 1;
		}
		return events;
	}

	private void createFromMessage(String body, List<GameEvent> events) {
		String[] parts = body.split(",");
		if (parts.length != FIELDS) return; // not a status message (eg. a <T,..> test line)

		int[] v = new int[FIELDS];
		int sum = 0;
		try {
			for (int i = 0; i < FIELDS; i++) {
				v[i] = Integer.parseInt(parts[i].trim());
				if (i < FIELDS - 1) sum += v[i];
			}
		} catch (NumberFormatException e) {
			return; // damaged message
		}
		int id = v[0], shots = v[1], hits = v[2], health = v[3], ammo = v[4], mag = v[5], reloads = v[6],
				check = v[7];
		if (sum != check) return; // damaged message
		if (id < 1 || id > MAX_PLAYERS || shots < 0 || hits < 0 || reloads < 0) return;
		boolean magazineIn = mag != 0;

		// first message, or the totals went down = the gun was reset / new match
		if (!seen[id] || shots < lastShots[id] || hits < lastHits[id] || reloads < lastReloads[id]) {
			seen[id] = true;
			lastShots[id] = shots;
			lastHits[id] = hits;
			lastReloads[id] = reloads;
			events.add(new StatusEvent(id, health, ammo, magazineIn, true));
			return;
		}

		while (lastShots[id] < shots) {
			lastShots[id]++;
			events.add(new FireEvent(id));
		}
		while (lastHits[id] < hits) {
			lastHits[id]++;
			events.add(new HitEvent(id, opponentOf(id)));
		}
		while (lastReloads[id] < reloads) {
			lastReloads[id]++;
			events.add(new ReloadEvent(id));
		}
		events.add(new StatusEvent(id, health, ammo, magazineIn, false));
	}

	public static int opponentOf(int id) {
		return id == 1 ? 2 : 1;
	}
}
