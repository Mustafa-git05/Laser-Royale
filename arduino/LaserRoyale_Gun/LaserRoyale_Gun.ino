/*
  =====================================================================
  LASER ROYALE - GUN / PLAYER NODE  (final game code)
  Board: Arduino Uno           Library: IRremote by Armin Joachimsmeyer 4.x
  =====================================================================

  Upload this SAME file to both guns. Only change PLAYER_ID below:
    gun 1 -> PLAYER_ID = 1        gun 2 -> PLAYER_ID = 2

  Wiring (see projectNew/BUILD_GUIDE.md and the images folder):
    D2  <- IR receiver KY-022  S   (receiver + 47 uF capacitor are on a mini
           breadboard on the player's vest, 3-wire cable to the gun: S, +, -)
    D4  <- trigger button module OUT
    D5  -> LilyPad Vibe Board  +
    D6  -> 1 kohm -> GREEN LED -> GND
    D7  -> 1 kohm -> RED LED   -> GND
    D8  -> active buzzer module I/O   (module is LOW-triggered)
    D9  -> 1 kohm -> BC107B base   (TSAL6100 IR emitter driver)
    D10 <- LilyPad XBee  tx      (XBee data into the Uno)
    D11 -> LilyPad XBee  rx      (Uno data out to the XBee)
    D12 <- reload sensor (TCRT5000 reflective IR module) OUT, looks into
           the magazine slot. Magazine in = white patch in front of it.

  Game rules on the gun:
    - Press the trigger = one blast. The blast is 3 copies of an NEC IR
      frame: address = my player ID, command = shot number (1..200).
    - A magazine holds MAGAZINE_SIZE blasts. Trigger with an empty
      magazine = click sound + red blink, no blast.
    - Reload: pull the magazine out (trigger locked, green LED blinks
      fast), keep it out at least RELOAD_MIN_MS, push it back in = full
      magazine + reload sound. Put back too early = no refill.
    - An IR frame from the OTHER player with a new shot number = one hit.
      Own frames (reflections) are ignored, copies of the same shot only
      count once.
    - Each hit removes DAMAGE_PER_HIT health. At 0 health the player is
      dead: red LED on, long buzz, the trigger is locked.
    - Green LED = alive (slow blink when health is low). Red LED = hit / dead.

  ZigBee messages (all text between < and >, anything else is ignored):
    gun -> PC   <id,shots,hitsTaken,health,ammo,mag,reloads,check>
                mag = 1 magazine in, 0 magazine out (reloading)
                check = sum of the 7 numbers before it
                sent after every change and once a second (running totals,
                so a lost message is fixed by the next one)
    PC  -> gun  <S,10>   start a new match with 10 health
                <W,2>    match over, player 2 won (win / lose sound)

  USB Serial Monitor (9600) for testing:  f = fire,  s = print status,
      n = new match,  r = pretend the magazine is pulled out / pushed in
      (only when RELOAD_SENSOR_FITTED is false, for bench tests)
  =====================================================================
*/

#if !defined(__AVR_ATmega328P__)
#error "Select Tools > Board > Arduino Uno"
#endif

// The IR receiver uses Timer2. Timer1 is left free for our own 38 kHz carrier on D9.
#define IR_USE_AVR_TIMER2
#include <IRremote.hpp>
#include <SoftwareSerial.h>

// =====================================================================
// SETTINGS
// =====================================================================
const uint8_t PLAYER_ID = 2;           // 1 on gun 1, 2 on gun 2
const uint8_t MAX_PLAYERS = 2;

const int DEFAULT_HEALTH = 10;         // the PC can change it with <S,n>
const int DAMAGE_PER_HIT = 1;
const int LOW_HEALTH = 3;              // green LED blinks at or below this

const int MAGAZINE_SIZE = 6;           // blasts per magazine
const unsigned long RELOAD_MIN_MS = 2000;   // magazine must stay out this long
// true once the reflective sensor is wired. false = no sensor yet, the
// 'r' key in the Serial Monitor pretends to pull / insert the magazine.
const bool RELOAD_SENSOR_FITTED = true;
// Level on the sensor OUT pin when the magazine (white patch) is in front
// of it. TCRT5000 modules give LOW when they see a reflection (D1 LED on).
// Test4_Reload tells you which level your module gives.
const int MAGAZINE_IN_LEVEL = LOW;

const uint8_t IR_POWER_PERCENT = 33;   // 38 kHz duty: 10 = weak, 50 = strongest
const uint8_t COPIES_PER_SHOT = 3;
const unsigned long FIRE_COOLDOWN_MS = 250;
const unsigned long STATUS_PERIOD_MS = 1000;

const bool BUZZER_ACTIVE_LOW = true;   // our module beeps when I/O is LOW

// =====================================================================
// PINS
// =====================================================================
const uint8_t PIN_IR_RECEIVE = 2;
const uint8_t PIN_BUTTON = 4;
const uint8_t PIN_VIBE = 5;
const uint8_t PIN_LED_GREEN = 6;
const uint8_t PIN_LED_RED = 7;
const uint8_t PIN_BUZZER = 8;
const uint8_t PIN_IR_SEND = 9;         // Timer1 output OC1A, must be D9
const uint8_t PIN_XBEE_RX = 10;        // Uno receives on D10 (from XBee tx)
const uint8_t PIN_XBEE_TX = 11;        // Uno sends on D11 (to XBee rx)
const uint8_t PIN_RELOAD = 12;         // reload sensor OUT

// =====================================================================
// FEEDBACK DEVICES  (interface from the Phase 2 class diagram)
// Every device gets the same calls, PlayerNode does not need to know
// which device is which. update() runs timed effects without delay().
// =====================================================================
class FeedbackDevice {
public:
  virtual void begin() = 0;
  virtual void playHit() = 0;
  virtual void showHealth(int health, int maxHealth) = 0;
  virtual void update() = 0;
  virtual void playFire() {}
  virtual void playEmpty() {}                    // trigger pulled, no ammo
  virtual void showReloading(bool reloading) {}  // magazine out / back in
  virtual void playReloadDone() {}
  virtual void playStart() {}
  virtual void playGameOver(bool won) {}
};

// ---------- green + red LED ----------
class StatusLed : public FeedbackDevice {
  uint8_t greenPin, redPin;
  int health = 1, maxHealth = 1;
  bool won = false, gameOver = false, reloading = false;
  unsigned long flashUntil = 0;
public:
  StatusLed(uint8_t g, uint8_t r) : greenPin(g), redPin(r) {}

  void begin() override {
    pinMode(greenPin, OUTPUT);
    pinMode(redPin, OUTPUT);
  }
  void playHit() override { flashUntil = millis() + 300; }   // red flash
  void playEmpty() override { flashUntil = millis() + 120; } // short red blink
  void showReloading(bool r) override { reloading = r; }
  void showHealth(int h, int m) override { health = h; maxHealth = m; }
  void playStart() override { gameOver = false; }
  void playGameOver(bool w) override { gameOver = true; won = w; }

  void update() override {
    unsigned long now = millis();
    bool green, red;
    if (health <= 0) {                       // dead
      green = false;
      red = true;
    } else if ((long)(now - flashUntil) < 0) {   // just got hit / empty click
      green = false;
      red = true;
    } else if (gameOver && won) {            // winner: fast green blink
      green = (now / 150) % 2;
      red = false;
    } else if (reloading) {                  // magazine out: fast green blink
      green = (now / 100) % 2;
      red = false;
    } else if (health <= LOW_HEALTH) {       // low health: slow green blink
      green = (now / 400) % 2;
      red = false;
    } else {                                 // alive
      green = true;
      red = false;
    }
    digitalWrite(greenPin, green);
    digitalWrite(redPin, red);
  }
};

// ---------- active buzzer ----------
// A sound is a list of times in ms: on, off, on, off ...
const uint16_t SOUND_FIRE[] = {40};
const uint16_t SOUND_EMPTY[] = {15};               // dry "click"
const uint16_t SOUND_RELOAD[] = {50, 50, 50, 50, 120};
const uint16_t SOUND_HIT[] = {80, 60, 80};
const uint16_t SOUND_DEAD[] = {1200};
const uint16_t SOUND_START[] = {100, 100, 100, 100, 400};
const uint16_t SOUND_WIN[] = {80, 60, 80, 60, 80, 60, 600};
const uint16_t SOUND_LOSE[] = {600, 200, 600};

class Buzzer : public FeedbackDevice {
  uint8_t pin;
  const uint16_t* sound = nullptr;
  uint8_t length = 0, step = 0;
  unsigned long stepEnd = 0;
  int lastHealth = -1;

  void output(bool on) {
    digitalWrite(pin, (on != BUZZER_ACTIVE_LOW) ? HIGH : LOW);
  }
  void play(const uint16_t* s, uint8_t len) {
    sound = s;
    length = len;
    step = 0;
    stepEnd = millis() + s[0];
    output(true);
  }
public:
  Buzzer(uint8_t p) : pin(p) {}

  void begin() override {
    output(false);                           // silent before the pin becomes an output
    pinMode(pin, OUTPUT);
    output(false);
  }
  void playHit() override { play(SOUND_HIT, 3); }
  void playFire() override { play(SOUND_FIRE, 1); }
  void playEmpty() override { play(SOUND_EMPTY, 1); }
  void playReloadDone() override { play(SOUND_RELOAD, 5); }
  void playStart() override { play(SOUND_START, 5); }
  void playGameOver(bool won) override {
    if (won) play(SOUND_WIN, 7);
    else play(SOUND_LOSE, 3);
  }
  void showHealth(int h, int m) override {
    if (h <= 0 && lastHealth > 0) play(SOUND_DEAD, 1);   // just died
    lastHealth = h;
  }
  void update() override {
    if (sound == nullptr || (long)(millis() - stepEnd) < 0) return;
    step++;
    if (step >= length) {
      sound = nullptr;
      output(false);
      return;
    }
    stepEnd = millis() + sound[step];
    output(step % 2 == 0);                   // even steps = on, odd = off
  }
};

// ---------- vibration motor (LilyPad Vibe Board) ----------
class VibrationMotor : public FeedbackDevice {
  uint8_t pin;
  unsigned long offAt = 0;
  int lastHealth = -1;
public:
  VibrationMotor(uint8_t p) : pin(p) {}

  void begin() override {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  void playHit() override {
    digitalWrite(pin, HIGH);
    offAt = millis() + 400;
  }
  void showHealth(int h, int m) override {
    if (h <= 0 && lastHealth > 0) {          // long buzz when killed
      digitalWrite(pin, HIGH);
      offAt = millis() + 1200;
    }
    lastHealth = h;
  }
  void update() override {
    if (offAt != 0 && (long)(millis() - offAt) >= 0) {
      digitalWrite(pin, LOW);
      offAt = 0;
    }
  }
};

// =====================================================================
// SENSORS  (abstract Sensor class from the Phase 2 class diagram)
// =====================================================================
class Sensor {
protected:
  uint8_t pin;
public:
  Sensor(uint8_t p) : pin(p) {}
  virtual void begin() = 0;
  virtual bool isTriggered() = 0;            // true once per new event
};

// ---------- trigger button module ----------
class PushButton : public Sensor {
  int idleLevel = LOW;
  bool lastPressed = false;
  unsigned long lastChange = 0;
public:
  PushButton(uint8_t p) : Sensor(p) {}

  void begin() override {
    pinMode(pin, INPUT);
    delay(50);
    idleLevel = digitalRead(pin);            // don't press the trigger while the gun starts
  }
  // true once for each press (with 30 ms debounce)
  bool isTriggered() override {
    bool pressed = digitalRead(pin) != idleLevel;
    if (pressed == lastPressed || millis() - lastChange < 30) return false;
    lastPressed = pressed;
    lastChange = millis();
    return pressed;
  }
};

// ---------- reload sensor: TCRT5000 reflective IR module ----------
// Looks into the magazine slot. Magazine in = its white patch reflects
// the sensor's IR back = "detected". The reading is debounced for 50 ms
// so a wobbling magazine doesn't flicker.
class PhotoelectricSensor : public Sensor {
  bool magazineIn = true;
  bool lastRaw = true;
  unsigned long rawSince = 0;
  bool changed = false;
public:
  PhotoelectricSensor(uint8_t p) : Sensor(p) {}

  void begin() override {
    pinMode(pin, INPUT);
    delay(50);
    magazineIn = lastRaw = readRaw();
    rawSince = millis();
  }
  bool readRaw() {
    if (!RELOAD_SENSOR_FITTED) return virtualIn;
    return digitalRead(pin) == MAGAZINE_IN_LEVEL;
  }
  bool isMagazineIn() { return magazineIn; }
  bool virtualIn = true;                   // used by the 'r' key when no sensor is fitted

  // true once each time the magazine goes out or comes in
  bool isTriggered() override {
    bool raw = readRaw();
    if (raw != lastRaw) {
      lastRaw = raw;
      rawSince = millis();
    }
    if (raw != magazineIn && millis() - rawSince >= 50) {
      magazineIn = raw;
      return true;
    }
    return false;
  }
};

// ---------- IR receiver KY-022 ----------
class IRReceiver : public Sensor {
  uint8_t ownId;
  uint8_t lastShotFrom[MAX_PLAYERS + 1];
  uint8_t shooter = 0;
  unsigned int noiseFrames = 0;
public:
  IRReceiver(uint8_t p, uint8_t id) : Sensor(p), ownId(id) {}

  void begin() override {
    IrReceiver.begin(pin, DISABLE_LED_FEEDBACK);
    clearHistory();
  }
  void clearHistory() {
    for (uint8_t i = 0; i <= MAX_PLAYERS; i++) lastShotFrom[i] = 0;
  }
  bool isIdle() { return IrReceiver.isIdle(); }
  uint8_t getShooter() { return shooter; }
  unsigned int getNoiseFrames() { return noiseFrames; }

  // true if a NEW blast from the other player arrived
  bool isTriggered() override {
    if (!IrReceiver.decode()) return false;

    decode_type_t protocol = IrReceiver.decodedIRData.protocol;
    uint16_t address = IrReceiver.decodedIRData.address;
    uint16_t command = IrReceiver.decodedIRData.command;
    bool overflow = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW;
    IrReceiver.resume();

    // NEC2 = an NEC frame less than 70 ms after the last one (our copies 2 and 3)
    bool ours = (protocol == NEC || protocol == NEC2) && !overflow &&
                address >= 1 && address <= MAX_PLAYERS &&
                command >= 1 && command <= 200;
    if (!ours) {
      noiseFrames++;
      return false;
    }
    if (address == ownId) return false;                 // my own blast bounced back
    if (command == lastShotFrom[address]) return false; // copy of a blast already counted

    lastShotFrom[address] = command;
    shooter = address;
    return true;
  }
};

// =====================================================================
// IR EMITTER  (TSAL6100 driven by the BC107B transistor on D9)
// Timer1 makes the 38 kHz carrier in hardware, so receiver interrupts
// can't disturb it. The duty cycle sets the power.
// =====================================================================
class IREmitter {
  unsigned long edgeTime = 0;

  void carrierOn() {
    TCNT1 = 0;
    TCCR1A |= _BV(COM1A1);                   // connect Timer1 to D9
  }
  void carrierOff() {
    TCCR1A &= ~(_BV(COM1A1) | _BV(COM1A0));  // disconnect, D9 back to normal pin
    digitalWrite(PIN_IR_SEND, LOW);
  }
  void waitUntil(unsigned long t) {
    while ((long)(micros() - t) < 0) {}
  }
  void mark(unsigned int us) { carrierOn(); edgeTime += us; waitUntil(edgeTime); }
  void space(unsigned int us) { carrierOff(); edgeTime += us; waitUntil(edgeTime); }
  void sendByte(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++) {
      mark(560);
      space((b & 1) ? 1690 : 560);
      b >>= 1;
    }
  }
public:
  void begin(uint8_t powerPercent) {
    pinMode(PIN_IR_SEND, OUTPUT);
    digitalWrite(PIN_IR_SEND, LOW);
    // Timer1 fast PWM mode 14: TOP = ICR1, no prescaler
    TCCR1A = _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
    ICR1 = (F_CPU / 38000UL) - 1;            // 420 on a 16 MHz Uno -> 38 kHz
    OCR1A = ((uint32_t)(ICR1 + 1) * powerPercent) / 100;
  }
  // one NEC frame: about 68 ms
  void sendNEC(uint8_t address, uint8_t command) {
    edgeTime = micros();
    mark(9000);
    space(4500);
    sendByte(address);
    sendByte(~address);
    sendByte(command);
    sendByte(~command);
    mark(560);
    carrierOff();
  }
};

// =====================================================================
// XBEE RADIO  (LilyPad XBee board on SoftwareSerial D10/D11)
// =====================================================================
class XBeeRadio {
  SoftwareSerial port;
  char buffer[16];
  uint8_t length = 0;
  bool inMessage = false;
public:
  char command = 0;
  int value = 0;

  XBeeRadio(uint8_t rx, uint8_t tx) : port(rx, tx) {}
  void begin() { port.begin(9600); }

  void sendStatus(uint8_t id, int shots, int hits, int health, int ammo, bool magIn, int reloads) {
    char line[48];
    int mag = magIn ? 1 : 0;
    snprintf(line, sizeof(line), "<%d,%d,%d,%d,%d,%d,%d,%d>", id, shots, hits, health, ammo, mag,
             reloads, id + shots + hits + health + ammo + mag + reloads);
    port.println(line);
  }

  // reads what arrived; true when a full <X,n> command is ready in command/value
  bool readCommand() {
    while (port.available()) {
      char c = port.read();
      if (c == '<') {
        inMessage = true;
        length = 0;
      } else if (c == '>' && inMessage) {
        inMessage = false;
        buffer[length] = 0;
        command = buffer[0];
        value = (length > 2 && buffer[1] == ',') ? atoi(buffer + 2) : 0;
        if (command == 'S' || command == 'W') return true;
      } else if (inMessage) {
        if (length < sizeof(buffer) - 1) buffer[length++] = c;
        else inMessage = false;              // too long, junk
      }
    }
    return false;
  }
};

// =====================================================================
// PLAYER NODE  (main class: game state + coordinates everything)
// =====================================================================
class PlayerNode {
  uint8_t id;
  PushButton& trigger;
  PhotoelectricSensor& magazine;
  IRReceiver& receiver;
  IREmitter& emitter;
  XBeeRadio& radio;
  FeedbackDevice** devices;
  uint8_t deviceCount;

  int maxHealth = DEFAULT_HEALTH;
  int health = DEFAULT_HEALTH;
  int shots = 0;
  int hitsTaken = 0;
  int ammo = MAGAZINE_SIZE;
  int reloads = 0;
  bool reloading = false;                  // magazine is out
  unsigned long magazineOutSince = 0;
  uint8_t shotNumber = 1;
  unsigned long lastShot = 0;
  unsigned long lastStatus = 0;
  bool statusDue = true;

  void showHealthOnAll() {
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->showHealth(health, maxHealth);
  }
  void updateDevices() {
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->update();
  }

  // look at the receiver; a frame from the other player is a hit
  void checkReceiver() {
    if (receiver.isTriggered()) takeHit(receiver.getShooter());
  }

public:
  PlayerNode(uint8_t playerId, PushButton& b, PhotoelectricSensor& m, IRReceiver& r, IREmitter& e,
             XBeeRadio& x, FeedbackDevice** d, uint8_t n)
    : id(playerId), trigger(b), magazine(m), receiver(r), emitter(e), radio(x), devices(d), deviceCount(n) {}

  bool isAlive() { return health > 0; }

  void begin() {
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->begin();
    trigger.begin();
    magazine.begin();
    receiver.begin();
    emitter.begin(IR_POWER_PERCENT);
    radio.begin();

    randomSeed(analogRead(A0) ^ micros());
    shotNumber = random(1, 201);             // new numbers after a reset, so old ones never clash

    startMatch(DEFAULT_HEALTH);
    Serial.print(F("LASER ROYALE gun, player "));
    Serial.println(id);
    Serial.println(F("USB commands: f = fire, s = status, n = new match, r = fake magazine out/in"));
    if (!magazine.isMagazineIn()) Serial.println(F("Magazine is OUT: insert it to fire"));
  }

  void startMatch(int newHealth) {
    if (newHealth < 1 || newHealth > 99) newHealth = DEFAULT_HEALTH;
    maxHealth = newHealth;
    health = newHealth;
    shots = 0;
    hitsTaken = 0;
    ammo = MAGAZINE_SIZE;
    reloads = 0;
    reloading = !magazine.isMagazineIn();
    magazineOutSince = millis();
    receiver.clearHistory();
    for (uint8_t i = 0; i < deviceCount; i++) {
      devices[i]->playStart();
      devices[i]->showReloading(reloading);
    }
    showHealthOnAll();
    statusDue = true;
    Serial.print(F("NEW MATCH, health "));
    Serial.println(health);
  }

  // magazine pulled out or pushed back in
  void magazineChanged() {
    if (!magazine.isMagazineIn()) {          // pulled out: reload starts
      reloading = true;
      magazineOutSince = millis();
      Serial.println(F("MAGAZINE OUT, reloading..."));
    } else {                                 // pushed back in
      reloading = false;
      if (millis() - magazineOutSince >= RELOAD_MIN_MS) {
        ammo = MAGAZINE_SIZE;
        reloads++;
        for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playReloadDone();
        Serial.print(F("RELOADED, ammo "));
        Serial.println(ammo);
      } else {
        Serial.println(F("Magazine back too early, not refilled"));
      }
    }
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->showReloading(reloading);
    statusDue = true;
  }

  void fire() {
    if (!isAlive()) {
      Serial.println(F("Dead, can't fire"));
      return;
    }
    if (millis() - lastShot < FIRE_COOLDOWN_MS) return;
    lastShot = millis();
    if (reloading || ammo <= 0) {            // no magazine / empty: click
      for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playEmpty();
      Serial.println(reloading ? F("click (reloading)") : F("click (empty, reload!)"));
      return;
    }
    ammo--;
    shots++;
    shotNumber = (shotNumber >= 200) ? 1 : shotNumber + 1;

    // ~270 ms: 3 copies, checking the receiver between them so an
    // incoming blast is not lost while we shoot
    for (uint8_t c = 0; c < COPIES_PER_SHOT; c++) {
      checkReceiver();
      emitter.sendNEC(id, shotNumber);
      delay(12);                             // receiver needs 8 ms of quiet to end a frame
      checkReceiver();
    }
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playFire();
    statusDue = true;
    Serial.print(F("FIRE  shot "));
    Serial.print(shots);
    Serial.print(F("  ammo left "));
    Serial.println(ammo);
  }

  void takeHit(uint8_t shooter) {
    if (!isAlive()) return;
    hitsTaken++;
    health -= DAMAGE_PER_HIT;
    if (health < 0) health = 0;
    if (health > 0) {
      for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playHit();
    }
    showHealthOnAll();
    statusDue = true;
    Serial.print(F("HIT by player "));
    Serial.print(shooter);
    Serial.print(F("  health "));
    Serial.println(health);
    if (!isAlive()) Serial.println(F("*** DEAD ***"));
  }

  void printStatus() {
    Serial.print(F("player "));
    Serial.print(id);
    Serial.print(F("  shots "));
    Serial.print(shots);
    Serial.print(F("  hits taken "));
    Serial.print(hitsTaken);
    Serial.print(F("  health "));
    Serial.print(health);
    Serial.print(F("/"));
    Serial.print(maxHealth);
    Serial.print(F("  ammo "));
    Serial.print(ammo);
    Serial.print(F("/"));
    Serial.print(MAGAZINE_SIZE);
    Serial.print(reloading ? F(" (magazine OUT)") : F(" (magazine in)"));
    Serial.print(F("  reloads "));
    Serial.print(reloads);
    Serial.print(F("  IR noise frames "));
    Serial.println(receiver.getNoiseFrames());
  }

  void loop() {
    if (trigger.isTriggered()) fire();
    if (magazine.isTriggered()) magazineChanged();
    checkReceiver();

    // commands from the PC
    if (radio.readCommand()) {
      if (radio.command == 'S') {
        startMatch(radio.value);
      } else if (radio.command == 'W') {
        bool won = (radio.value == id);
        for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playGameOver(won);
        Serial.println(won ? F("WE WON") : F("WE LOST"));
      }
    }

    // commands typed in the USB Serial Monitor (testing)
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'f') fire();
      if (c == 's') printStatus();
      if (c == 'n') startMatch(maxHealth);
      if (c == 'r') {
        if (RELOAD_SENSOR_FITTED) Serial.println(F("Pull the real magazine out and push it back in"));
        else magazine.virtualIn = !magazine.virtualIn;   // isTriggered() picks it up
      }
    }

    // status to the PC: after a change and once a second.
    // Wait until no IR frame is coming in, because SoftwareSerial
    // stops interrupts while it sends and would spoil the frame.
    if ((statusDue || millis() - lastStatus >= STATUS_PERIOD_MS) && receiver.isIdle()) {
      radio.sendStatus(id, shots, hitsTaken, health, ammo, !reloading, reloads);
      lastStatus = millis();
      statusDue = false;
    }

    updateDevices();
  }
};

// =====================================================================
// OBJECTS
// =====================================================================
StatusLed statusLed(PIN_LED_GREEN, PIN_LED_RED);
Buzzer buzzer(PIN_BUZZER);
VibrationMotor vibration(PIN_VIBE);
FeedbackDevice* feedback[] = {&statusLed, &buzzer, &vibration};

PushButton trigger(PIN_BUTTON);
PhotoelectricSensor magazineSensor(PIN_RELOAD);
IRReceiver irReceiver(PIN_IR_RECEIVE, PLAYER_ID);
IREmitter irEmitter;
XBeeRadio xbee(PIN_XBEE_RX, PIN_XBEE_TX);

PlayerNode player(PLAYER_ID, trigger, magazineSensor, irReceiver, irEmitter, xbee, feedback, 3);

void setup() {
  Serial.begin(9600);
  player.begin();
}

void loop() {
  player.loop();
}
