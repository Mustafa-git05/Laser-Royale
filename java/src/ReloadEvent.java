/**
 * A player pulled the magazine out, kept it out long enough and pushed it
 * back in: the gun refilled the magazine. (The gun reports the running
 * number of reloads; the factory creates one ReloadEvent per new reload.)
 */
public class ReloadEvent extends GameEvent {

	public ReloadEvent(int playerId) {
		super(playerId);
	}

	@Override
	public boolean isScoring() {
		return true; // only counted while the match is running
	}

	@Override
	public void apply(Match match) {
		match.getStats(playerId).addReload();
	}

	@Override
	public String describe() {
		return "Player " + playerId + " reloaded";
	}
}
