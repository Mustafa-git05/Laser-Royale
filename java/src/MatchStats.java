/**
 * The numbers of ONE player in ONE match (association class between
 * Match and Player in the Phase 2 class diagram).
 */
public class MatchStats {

	private final int maxHealth;
	private int health;
	private int shots;
	private int hitsLanded;
	private int hitsTaken;
	private int reloads;
	private int gunHealth = -1;   // what the gun itself reports (-1 = nothing yet)
	private int ammo = -1;        // ammo left in the gun's magazine (-1 = nothing yet)
	private boolean magazineIn = true;

	public MatchStats(int maxHealth) {
		this.maxHealth = maxHealth;
		this.health = maxHealth;
	}

	public void addShot() {
		shots++;
	}

	public void addHitLanded() {
		hitsLanded++;
	}

	public void addReload() {
		reloads++;
	}

	public void takeHit(int damage) {
		hitsTaken++;
		health = Math.max(0, health - damage);
	}

	public void setGunHealth(int gunHealth) {
		this.gunHealth = gunHealth;
	}

	public void setAmmo(int ammo, boolean magazineIn) {
		this.ammo = ammo;
		this.magazineIn = magazineIn;
	}

	public boolean isEliminated() {
		return health <= 0;
	}

	/** hits landed / shots fired, in percent */
	public int getAccuracy() {
		return shots == 0 ? 0 : Math.round(100f * hitsLanded / shots);
	}

	public int getMaxHealth() {
		return maxHealth;
	}

	public int getHealth() {
		return health;
	}

	public int getShots() {
		return shots;
	}

	public int getHitsLanded() {
		return hitsLanded;
	}

	public int getHitsTaken() {
		return hitsTaken;
	}

	public int getReloads() {
		return reloads;
	}

	public int getGunHealth() {
		return gunHealth;
	}

	public int getAmmo() {
		return ammo;
	}

	public boolean isMagazineIn() {
		return magazineIn;
	}
}
