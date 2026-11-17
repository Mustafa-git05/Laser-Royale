import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Font;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.text.SimpleDateFormat;
import java.util.Date;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JProgressBar;
import javax.swing.JScrollPane;
import javax.swing.JSpinner;
import javax.swing.JTextArea;
import javax.swing.SpinnerNumberModel;
import javax.swing.SwingConstants;
import javax.swing.SwingUtilities;

/**
 * The scoreboard window the Game Host looks at (Phase 2 class diagram).
 * It is a concrete OBSERVER of the GameController (the Subject): it
 * registers itself in its constructor and the controller calls update()
 * after every change. Updates come from other threads, so every change to
 * the window is passed to the Swing thread with SwingUtilities.invokeLater().
 */
public class Dashboard extends JFrame implements Observer {

	private static final long serialVersionUID = 1L;
	private static final Color DARK = new Color(29, 53, 87);
	private static final Color GOOD = new Color(46, 160, 67);
	private static final Color BAD = new Color(214, 40, 40);

	private final transient GameController controller;
	private final JLabel matchLabel = new JLabel();
	private final JLabel stateLabel = new JLabel();
	private final JLabel winnerLabel = new JLabel(" ", SwingConstants.CENTER);
	private final PlayerPanel[] panels = new PlayerPanel[3];
	private final JTextArea logArea = new JTextArea(12, 70);
	private final JSpinner healthSpinner =
			new JSpinner(new SpinnerNumberModel(GameController.DEFAULT_HEALTH, 1, 99, 1));
	private final SimpleDateFormat clock = new SimpleDateFormat("HH:mm:ss");

	public Dashboard(GameController controller, String portName) {
		super("Laser Royale - Game Controller   (" + portName + ")");
		this.controller = controller;
		setDefaultCloseOperation(EXIT_ON_CLOSE);
		setLayout(new BorderLayout(10, 10));

		// ----- top bar: match info + start button -----
		JPanel top = new JPanel(new BorderLayout());
		top.setBackground(DARK);
		top.setBorder(BorderFactory.createEmptyBorder(10, 15, 10, 15));
		JPanel info = new JPanel(new GridLayout(2, 1));
		info.setOpaque(false);
		matchLabel.setForeground(Color.WHITE);
		matchLabel.setFont(new Font("SansSerif", Font.BOLD, 22));
		stateLabel.setForeground(new Color(200, 220, 245));
		stateLabel.setFont(new Font("SansSerif", Font.PLAIN, 16));
		info.add(matchLabel);
		info.add(stateLabel);
		top.add(info, BorderLayout.WEST);

		JPanel controls = new JPanel();
		controls.setOpaque(false);
		JLabel healthText = new JLabel("Health:");
		healthText.setForeground(Color.WHITE);
		JButton startButton = new JButton("Start new match");
		startButton.setFont(new Font("SansSerif", Font.BOLD, 16));
		startButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				controller.startMatch((Integer) healthSpinner.getValue());
			}
		});
		final JCheckBox rawBox = new JCheckBox("show raw ZigBee lines");
		rawBox.setOpaque(false);
		rawBox.setForeground(Color.WHITE);
		rawBox.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				controller.setShowRawLines(rawBox.isSelected());
			}
		});
		controls.add(rawBox);
		controls.add(healthText);
		controls.add(healthSpinner);
		controls.add(startButton);
		top.add(controls, BorderLayout.EAST);
		add(top, BorderLayout.NORTH);

		// ----- middle: the two players + winner -----
		JPanel middle = new JPanel(new BorderLayout(10, 10));
		middle.setBorder(BorderFactory.createEmptyBorder(0, 15, 0, 15));
		JPanel players = new JPanel(new GridLayout(1, 2, 15, 0));
		for (Player p : controller.getPlayers()) {
			panels[p.getId()] = new PlayerPanel(p);
			players.add(panels[p.getId()]);
		}
		middle.add(players, BorderLayout.CENTER);
		winnerLabel.setFont(new Font("SansSerif", Font.BOLD, 30));
		winnerLabel.setForeground(DARK);
		middle.add(winnerLabel, BorderLayout.SOUTH);
		add(middle, BorderLayout.CENTER);

		// ----- bottom: event log -----
		logArea.setEditable(false);
		logArea.setFont(new Font("Monospaced", Font.PLAIN, 13));
		JScrollPane scroll = new JScrollPane(logArea);
		scroll.setBorder(BorderFactory.createTitledBorder("Event log"));
		add(scroll, BorderLayout.SOUTH);

		refresh(controller.getMatch());
		pack();
		setLocationRelativeTo(null);

		controller.registerObserver(this);   // like the lecture's ConcreteObserver
	}

	// ---------- Observer ----------
	@Override
	public void update(final Match match, final String message) {
		SwingUtilities.invokeLater(new Runnable() {
			public void run() {
				if (message != null && !message.isEmpty()) {
					logArea.append(clock.format(new Date()) + "  " + message + "\n");
					logArea.setCaretPosition(logArea.getDocument().getLength());
				}
				refresh(match);
			}
		});
	}

	private void refresh(Match match) {
		synchronized (match) {
			matchLabel.setText("LASER ROYALE   Match #" + match.getMatchId());
			stateLabel.setText(match.getStateName() + "    time " + match.getSecondsPlayed() + " s");
			if (match.isOver()) {
				winnerLabel.setText("WINNER: " + match.getPlayer(match.getWinnerId()).getName());
			} else if (match.isRunning()) {
				winnerLabel.setText(" ");
			} else {
				winnerLabel.setText("Press \"Start new match\" when both guns are sending");
			}
			for (int id = 1; id <= 2; id++) {
				panels[id].show(match.getStats(id), match.isOver() && match.getWinnerId() == id);
			}
		}
	}

	// ---------- one box per player ----------
	private static class PlayerPanel extends JPanel {

		private static final long serialVersionUID = 1L;
		private final transient Player player;
		private final JLabel connection = new JLabel();
		private final JProgressBar healthBar = new JProgressBar();
		private final JLabel ammo = new JLabel();
		private final JLabel shots = new JLabel();
		private final JLabel landed = new JLabel();
		private final JLabel taken = new JLabel();
		private final JLabel accuracy = new JLabel();
		private final JLabel reloads = new JLabel();
		private final JLabel gunHealth = new JLabel();

		PlayerPanel(Player player) {
			this.player = player;
			setLayout(new GridLayout(9, 1, 4, 4));
			setBorder(BorderFactory.createCompoundBorder(
					BorderFactory.createTitledBorder(BorderFactory.createLineBorder(DARK, 2),
							player.getName() + "   (XBee " + player.getXbeeLabel() + ")", 0, 0,
							new Font("SansSerif", Font.BOLD, 20), DARK),
					BorderFactory.createEmptyBorder(8, 12, 8, 12)));
			healthBar.setStringPainted(true);
			healthBar.setFont(new Font("SansSerif", Font.BOLD, 18));
			Font big = new Font("SansSerif", Font.PLAIN, 18);
			for (JLabel l : new JLabel[] { connection, ammo, shots, landed, taken, accuracy, reloads, gunHealth }) {
				l.setFont(big);
			}
			ammo.setFont(new Font("SansSerif", Font.BOLD, 18));
			gunHealth.setFont(new Font("SansSerif", Font.ITALIC, 13));
			add(connection);
			add(healthBar);
			add(ammo);
			add(shots);
			add(landed);
			add(taken);
			add(accuracy);
			add(reloads);
			add(gunHealth);
		}

		void show(MatchStats s, boolean winner) {
			boolean on = player.isConnected();
			connection.setText(on ? "● gun connected" : "○ no signal from gun");
			connection.setForeground(on ? GOOD : BAD);

			healthBar.setMaximum(s.getMaxHealth());
			healthBar.setValue(s.getHealth());
			healthBar.setString(s.isEliminated() ? "ELIMINATED"
					: "Health " + s.getHealth() + " / " + s.getMaxHealth() + (winner ? "   WINNER" : ""));
			float part = (float) s.getHealth() / s.getMaxHealth();
			healthBar.setForeground(part > 0.5f ? GOOD : part > 0.25f ? new Color(230, 160, 0) : BAD);

			if (s.getAmmo() < 0) {
				ammo.setText("Ammo:  -");
				ammo.setForeground(Color.DARK_GRAY);
			} else if (!s.isMagazineIn()) {
				ammo.setText("Ammo:  RELOADING...");
				ammo.setForeground(new Color(230, 160, 0));
			} else {
				StringBuilder bullets = new StringBuilder();
				for (int i = 0; i < GameController.MAGAZINE_SIZE; i++) {
					bullets.append(i < s.getAmmo() ? "■ " : "□ ");
				}
				ammo.setText("Ammo:  " + s.getAmmo() + " / " + GameController.MAGAZINE_SIZE + "   " + bullets
						+ (s.getAmmo() == 0 ? "  EMPTY, reload!" : ""));
				ammo.setForeground(s.getAmmo() == 0 ? BAD : Color.DARK_GRAY);
			}
			shots.setText("Shots fired:  " + s.getShots());
			landed.setText("Hits landed:  " + s.getHitsLanded());
			taken.setText("Hits taken:   " + s.getHitsTaken());
			accuracy.setText("Accuracy:     " + s.getAccuracy() + " %");
			reloads.setText("Reloads:      " + s.getReloads());
			gunHealth.setText(s.getGunHealth() < 0 ? "health shown on the gun: -"
					: "health shown on the gun: " + s.getGunHealth());
		}
	}
}
