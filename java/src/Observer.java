/**
 * OBSERVER design pattern, Observer side (same as the lecture slides).
 *
 * The update() function is the contract between the Subject and all its
 * Observers. Like the lecture's update(Date d), ours passes the data the
 * observers need: the current Match and a one-line message for the log.
 *
 * Concrete observers in Laser Royale: Dashboard (the scoreboard window)
 * and ConsoleLogger (the Eclipse console). Each registers itself with the
 * Subject in its constructor, like the lecture's ConcreteObserver.
 * (In the Phase 2 class diagram this interface was called GameObserver.)
 */
public interface Observer {

	/**
	 * @param match   the current match (read several values inside
	 *                synchronized (match) if they must belong together)
	 * @param message a line for the log, or null when only the screen should refresh
	 */
	void update(Match match, String message);
}
