import java.text.SimpleDateFormat;
import java.util.Date;

/**
 * Concrete Observer that prints every game change in the Eclipse console.
 * Like the lecture's ConcreteObserver, it registers itself with the Subject
 * in its constructor.
 */
public class ConsoleLogger implements Observer {

	private final SimpleDateFormat clock = new SimpleDateFormat("HH:mm:ss");

	public ConsoleLogger(Subject subject) {
		subject.registerObserver(this);
	}

	@Override
	public void update(Match match, String message) {
		if (message != null && !message.isEmpty()) {
			System.out.println(clock.format(new Date()) + "  " + message);
		}
	}
}
