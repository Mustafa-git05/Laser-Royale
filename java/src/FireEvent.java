/** A player pressed the trigger and sent an IR blast. */
public class FireEvent extends GameEvent {

	public FireEvent(int playerId) {
		super(playerId);
	}

	@Override
	public boolean isScoring() {
		return true;
	}

	@Override
	public void apply(Match match) {
		match.getStats(playerId).addShot();
	}

	@Override
	public String describe() {
		return "Player " + playerId + " fired";
	}
}
