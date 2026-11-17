/**
 * A status message arrived from a gun. It doesn't change the score, it
 * only tells us the gun is still connected and what the gun shows right
 * now: its health, the ammo left and whether the magazine is in.
 * joined = true for the first message, or after the gun was reset.
 */
public class StatusEvent extends GameEvent {

	private final int gunHealth;
	private final int ammo;
	private final boolean magazineIn;
	private final boolean joined;

	public StatusEvent(int playerId, int gunHealth, int ammo, boolean magazineIn, boolean joined) {
		super(playerId);
		this.gunHealth = gunHealth;
		this.ammo = ammo;
		this.magazineIn = magazineIn;
		this.joined = joined;
	}

	public boolean isJoined() {
		return joined;
	}

	@Override
	public boolean isScoring() {
		return false;
	}

	@Override
	public void apply(Match match) {
		MatchStats s = match.getStats(playerId);
		s.setGunHealth(gunHealth);
		s.setAmmo(ammo, magazineIn);
	}

	@Override
	public String describe() {
		return joined ? "Player " + playerId + " gun connected / reset (health " + gunHealth + ", ammo " + ammo + ")"
				: "Player " + playerId + " status";
	}
}
