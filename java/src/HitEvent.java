/**
 * A player's IR receiver picked up a blast from the opponent.
 * playerId = the player who was hit, shooterId = the player who scored.
 */
public class HitEvent extends GameEvent {

	private final int shooterId;

	public HitEvent(int playerId, int shooterId) {
		super(playerId);
		this.shooterId = shooterId;
	}

	public int getShooterId() {
		return shooterId;
	}

	@Override
	public boolean isScoring() {
		return true;
	}

	@Override
	public void apply(Match match) {
		match.getStats(playerId).takeHit(match.getDamagePerHit());
		match.getStats(shooterId).addHitLanded();
	}

	@Override
	public String describe() {
		return "Player " + playerId + " was hit by player " + shooterId;
	}
}
