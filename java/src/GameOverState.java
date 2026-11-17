/** Somebody won: the score is frozen until a new match starts. */
public class GameOverState implements MatchState {

	@Override
	public String getName() {
		return "GAME OVER";
	}

	@Override
	public void handle(Match match, GameEvent event) {
		if (!event.isScoring()) event.apply(match);
	}
}
