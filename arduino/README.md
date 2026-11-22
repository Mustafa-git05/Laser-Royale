# Laser Royale – Arduino Blaster Code

`LaserRoyale_Gun/LaserRoyale_Gun.ino` runs on each player's **Arduino Uno**. The same file goes on both blasters: only `PLAYER_ID` changes.

The blaster fires coded IR shots, detects hits on the chest receiver, handles the magazine and reload, drives the LEDs, buzzer and vibration motor, and reports to the PC over ZigBee.

## Requirements
- **Arduino IDE** (2.x)
- **Board:** Arduino Uno
- **Library:** **IRremote** by *shirriff, z3t0, ArminJo* (version 4.x). Install it from **Tools > Manage Libraries**. Don't install IRremoteESP8266.
- `SoftwareSerial` is built into the Arduino IDE, so nothing to install.

## Upload
1. Open `LaserRoyale_Gun/LaserRoyale_Gun.ino` in the Arduino IDE.
2. Select **Tools > Board > Arduino Uno** and the blaster's port under **Tools > Port**.
3. Set the player number near the top of the file:
   ```cpp
   const uint8_t PLAYER_ID = 1;   // 1 on blaster 1, 2 on blaster 2
   ```
4. Click **Upload**. Then set `PLAYER_ID = 2` and upload to the second blaster.
5. **Don't press the trigger while the board starts.** The code reads the button's resting level at start-up.

## Wiring (same on both blasters)

| Uno pin | Component | Notes |
|---|---|---|
| D2 | KY-022 IR receiver `S` (chest) | 3-wire cable to the vest. 47 µF capacitor across the receiver's + and − |
| D4 | Push button module `OUT` | Trigger |
| D5 | LilyPad Vibe Board `+` | Vibration motor |
| D6 | Green LED (through 1 kΩ) | |
| D7 | Red LED (through 1 kΩ) | |
| D8 | Active buzzer module `I/O` | Low-level trigger |
| D9 | BC107B base (through 1 kΩ) | Drives the TSAL6100 IR LED (33 Ω to 5 V). Must be D9: Timer1 makes the 38 kHz carrier |
| D10 | LilyPad XBee `tx` | XBee data into the Uno |
| D11 | LilyPad XBee `rx` | Uno data out to the XBee |
| D12 | TCRT5000 reload sensor `OUT` | Looks into the magazine slot |
| 5V / GND | + and − rails | Power for all modules and the XBee board |

Power the Uno from a **USB power bank** into its USB port (not the round barrel jack).

## Settings (top of the file)

| Setting | Default | What it does |
|---|---|---|
| `PLAYER_ID` | 1 or 2 | Which player this blaster is |
| `DEFAULT_HEALTH` | 10 | Starting health (the PC can change it when it starts a match) |
| `DAMAGE_PER_HIT` | 1 | Health lost per hit |
| `LOW_HEALTH` | 3 | Green LED blinks slowly at or below this |
| `MAGAZINE_SIZE` | 6 | Shots per magazine |
| `RELOAD_MIN_MS` | 2000 | Magazine must stay out this long to refill |
| `RELOAD_SENSOR_FITTED` | true | `false` = no sensor yet: use the `r` key to fake the magazine |
| `MAGAZINE_IN_LEVEL` | LOW | Sensor output when the magazine is in |
| `IR_POWER_PERCENT` | 33 | IR strength, 10 (weak) to 50 (strongest). Tunes the range |
| `COPIES_PER_SHOT` | 3 | IR frames sent per shot |
| `FIRE_COOLDOWN_MS` | 250 | Minimum time between shots |
| `STATUS_PERIOD_MS` | 1000 | How often the status is sent to the PC |
| `BUZZER_ACTIVE_LOW` | true | Set `false` if your buzzer beeps when the pin is HIGH |

## ZigBee messages

| Direction | Format | Example |
|---|---|---|
| Blaster → PC | `<id,shots,hitsTaken,health,ammo,mag,reloads,check>` | `<1,12,3,7,4,1,2,30>` |
| PC → blaster | `<S,health>` start a new match | `<S,10>` |
| PC → blaster | `<W,id>` match over, winner | `<W,2>` |

The status line is sent after every event and once a second. It carries **running totals**, so a lost message is fixed by the next one. `mag` = 1 magazine in, 0 out. `check` = the sum of the other 7 numbers, so the PC can reject damaged messages.

## Testing from the Serial Monitor (9600 baud)

| Key | Action |
|---|---|
| `f` | Fire one shot |
| `s` | Print the status (health, ammo, magazine, reloads, IR noise count) |
| `n` | Start a new match |
| `r` | Pretend the magazine is pulled out / pushed in (only when `RELOAD_SENSOR_FITTED = false`) |

## Code structure
- `PlayerNode` runs the blaster.
- **Sensors:** `PushButton` (trigger), `PhotoelectricSensor` (reload sensor), `IRReceiver` (hits).
- **Feedback:** `StatusLed`, `Buzzer`, `VibrationMotor`, all through the `FeedbackDevice` interface.
- **Output:** `IREmitter` sends the IR shot, `XBeeRadio` handles the ZigBee link.

Nothing in the feedback uses `delay()`, so the blaster never stops listening for hits.
