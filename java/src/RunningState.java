/** The match is on: every shot and hit counts, and after each one we check for a winner. */
public class RunningState implements MatchState {

	@Override
	public String getName() {
		return "MATCH RUNNING";
	}

	@Override
	public void handle(Match match, GameEvent event) {
		event.apply(match);
		match.checkForWinner();
	}
}
