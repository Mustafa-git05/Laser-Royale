/**
 * OBSERVER design pattern, Subject side (same as the lecture slides).
 *
 * The Subject is the object that generates data. Observers register with
 * it, can leave again, and are all notified when something changes.
 * In Laser Royale the Subject is the GameController: it generates the game
 * events (shots, hits, reloads, connections, game over).
 */
public interface Subject {

	void registerObserver(Observer o);

	void removeObserver(Observer o);

	/** calls update(...) on every registered observer */
	void notifyObservers(String message);
}
