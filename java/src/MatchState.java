/**
 * STATE design pattern: a match is always in one of three states, and the
 * state decides what an incoming event is allowed to do.
 *
 *   WaitingState  --start-->  RunningState  --someone at 0 health-->  GameOverState
 *
 * Pressing "Start new match" always creates a new Match in RunningState.
 */
public interface MatchState {

	String getName();

	/** what happens to an event in this state */
	void handle(Match match, GameEvent event);
}
