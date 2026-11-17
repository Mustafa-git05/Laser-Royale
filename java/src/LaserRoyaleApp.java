import javax.swing.JOptionPane;
import javax.swing.SwingUtilities;

import jssc.SerialPortException;
import jssc.SerialPortList;

/**
 * Start here (Run As > Java Application).
 *
 * 1) Plug the XBee Explorer USB (with the 22 RX coordinator) into the PC.
 * 2) Close the Arduino Serial Monitor if it uses that port.
 * 3) Run this, pick the Explorer's COM port, switch on both guns, then
 *    press "Start new match".
 *
 * You can also give the port as a program argument, eg. COM5.
 */
public class LaserRoyaleApp {

	public static void main(String[] args) {
		String portName = args.length > 0 ? args[0] : askForPort();
		if (portName == null) return;

		final GameController controller = GameController.getInstance();   // the Subject
		new ConsoleLogger(controller);                                    // Observer 1 registers itself

		try {
			XBeeLink link = new XBeeLink(portName, controller);
			controller.setLink(link);
			link.start();
		} catch (SerialPortException e) {
			JOptionPane.showMessageDialog(null, "Could not open " + portName + ":\n" + e.getMessage()
					+ "\n\nIs the Explorer plugged in? Is the port open in another program?",
					"Laser Royale", JOptionPane.ERROR_MESSAGE);
			return;
		}

		final String shownPort = portName;
		SwingUtilities.invokeLater(new Runnable() {
			public void run() {
				Dashboard dashboard = new Dashboard(controller, shownPort);   // Observer 2 registers itself
				dashboard.setVisible(true);
			}
		});
		controller.start();
	}

	/** asks which COM port the XBee Explorer is on */
	static String askForPort() {
		String[] ports = SerialPortList.getPortNames();
		if (ports.length == 0) {
			JOptionPane.showMessageDialog(null, "No COM port found. Is the XBee Explorer plugged in?",
					"Laser Royale", JOptionPane.ERROR_MESSAGE);
			return null;
		}
		return (String) JOptionPane.showInputDialog(null, "Which COM port is the XBee Explorer on?",
				"Laser Royale", JOptionPane.QUESTION_MESSAGE, null, ports, ports[0]);
	}
}
