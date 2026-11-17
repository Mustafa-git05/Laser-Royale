/** Before the host presses Start: guns can connect and test-fire, nothing is scored. */
public class WaitingState implements MatchState {

	@Override
	public String getName() {
		return "WAITING FOR START";
	}

	@Override
	public void handle(Match match, GameEvent event) {
		if (!event.isScoring()) event.apply(match);
	}
}
