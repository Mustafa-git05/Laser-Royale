# Laser Royale – Java Game Controller

The Java program on the PC that receives the two guns' messages over ZigBee, runs the match and shows the live scoreboard.

```
java/
  src/   all the Java source files (default package)
  lib/   jssc-2.9.6.jar – the serial port library (same as Lab 8)
```

## Requirements
- **Eclipse IDE** for Java.
- **Java SE 19 or below** as the execution environment.
- The **XBee Explorer USB** with the coordinator XBee (RX), plugged into the PC.

## 1. Create the project
1. Launch Eclipse.
2. **File > New > Java Project**, name it `LaserRoyaleController`.
3. Make sure the execution environment selected is **JavaSE-19 or below**.
4. If Eclipse asks to create `module-info.java`, click **Don't Create**.
5. Click **Finish**.

## 2. Add the source files
1. Copy **all** the `.java` files from `src/` into the project's `src` folder in Eclipse (drag them onto `src` in the Project Explorer and choose **Copy files**).
2. They must sit directly in `src` ("default package"), not inside another folder.

## 3. Add the Serial Library (jssc-2.9.6.jar)


1. Right-click your project in the **Project Explorer > Build Path > Configure Build Path…**
2. Select **Libraries > Classpath > Add External JARs…**
3. Select **`lib/jssc-2.9.6.jar`** from this folder > **Open**.
4. Click **Apply and Close**.

The project should now show **Referenced Libraries > jssc-2.9.6.jar**, and no file should have a red **X**.

## 4. Find the XBee Explorer's COM port
1. Plug the XBee Explorer (with RX) into the PC.
2. Open **Device Manager > Ports (COM & LPT)**, or **Arduino IDE > Tools > Port**, and note the port, e.g. `COM7`.
3. **Close the Arduino Serial Monitor** if it's open on that port. Only one program can use a COM port at a time.

## 5. Run the game
1. In the Project Explorer, right-click **`LaserRoyaleApp.java` > Run As > Java Application**.
2. Pick the Explorer's COM port in the popup.
3. Switch on both guns. Each player panel should say **gun connected**.
4. Set the health and press **Start new match**.

The scoreboard shows each player's health, ammo, shots, hits, accuracy and reloads, and announces the winner. Every event is also printed in the Eclipse console.

## Troubleshooting
| Problem | Fix |
|---|---|
| Red X on the files / `jssc cannot be resolved` | The jar isn't on the build path: redo step 3. |
| Only "Ant Build" appears under Run As | The files aren't in a Java project's `src`: redo steps 1–2 and run from the Project Explorer. |
| "Could not open COMx" / port busy | Wrong port, or the port is open in another program (close the Arduino Serial Monitor). |
| No COM port in the list | The Explorer isn't plugged in, or its USB driver isn't installed. |
| A gun shows "no signal" | Check that gun is on and its XBee is connected (tx → D10, rx → D11). |
