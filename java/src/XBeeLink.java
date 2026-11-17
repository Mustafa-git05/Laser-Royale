import jssc.SerialPort;
import jssc.SerialPortException;
import jssc.SerialPortTimeoutException;

/**
 * Connection to the coordinator XBee (22 RX) in the XBee Explorer USB.
 *
 * A separate READER THREAD reads the serial port all the time, cuts the
 * bytes into lines, turns each line into events with the EventFactory and
 * hands the events to the GameController's queue. The game logic runs on
 * another thread, so a slow screen never makes us miss serial data.
 */
public class XBeeLink implements Runnable {

	private final String portName;
	private final GameController controller;
	private final EventFactory factory = new EventFactory();
	private final SerialPort port;
	private volatile boolean running = false;

	public XBeeLink(String portName, GameController controller) throws SerialPortException {
		this.portName = portName;
		this.controller = controller;
		port = new SerialPort(portName);
		port.openPort();
		port.setParams(9600, 8, 1, 0); // same speed as the XBees, 8 data bits, 1 stop bit, no parity
		port.purgePort(SerialPort.PURGE_RXCLEAR | SerialPort.PURGE_TXCLEAR);
	}

	public String getPortName() {
		return portName;
	}

	/** starts the reader thread */
	public void start() {
		running = true;
		Thread reader = new Thread(this, "XBee reader");
		reader.setDaemon(true);
		reader.start();
	}

	/** the reader thread: one byte at a time, a line ends at \n or \r */
	@Override
	public void run() {
		StringBuilder line = new StringBuilder();
		while (running) {
			try {
				byte[] data = port.readBytes(1, 500); // waits max 0.5 s for a byte
				char c = (char) (data[0] & 0xFF);
				if (c == '\n' || c == '\r') {
					if (line.length() > 0) {
						onLine(line.toString());
						line.setLength(0);
					}
				} else {
					line.append(c);
					if (line.length() > 200) line.setLength(0); // garbage without a line end
				}
			} catch (SerialPortTimeoutException e) {
				// nothing arrived in 0.5 s, just keep waiting
			} catch (SerialPortException e) {
				controller.log("Serial port error: " + e.getMessage());
				running = false;
			}
		}
	}

	/** one complete line from the guns */
	void onLine(String line) {
		controller.rawLine(line);
		for (GameEvent event : factory.createEvents(line)) {
			controller.submit(event);
		}
	}

	/** sends a command to the guns, eg. "<S,10>" */
	public synchronized void send(String command) {
		try {
			port.writeString(command + "\r\n");
		} catch (SerialPortException e) {
			controller.log("Could not send " + command + ": " + e.getMessage());
		}
	}

	public void close() {
		running = false;
		try {
			port.closePort();
		} catch (SerialPortException e) {
			// closing anyway
		}
	}
}
