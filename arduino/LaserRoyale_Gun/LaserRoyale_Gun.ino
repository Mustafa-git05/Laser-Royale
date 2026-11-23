/*
  =====================================================================
  LASER ROYALE - GUN / PLAYER NODE  (final game code)
  Board: Arduino Uno           Library: IRremote by Armin Joachimsmeyer 4.x
  =====================================================================

  Upload this SAME file to both guns. Only change PLAYER_ID below:
    gun 1 -> PLAYER_ID = 1        gun 2 -> PLAYER_ID = 2

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
  =====================================================================
*/

// Stop the upload with a clear error if the wrong board is selected
#if !defined(__AVR_ATmega328P__)
#error "Select Tools > Board > Arduino Uno"
#endif

// The IR receiver uses Timer2. Timer1 is left free for our own 38 kHz carrier on D9.
#define IR_USE_AVR_TIMER2
#include <IRremote.hpp>        // library that decodes the IR frames we receive
#include <SoftwareSerial.h>    // lets us talk to the XBee on normal pins D10/D11

// =====================================================================
// SETTINGS
// =====================================================================
const uint8_t PLAYER_ID = 2;           // 1 on gun 1, 2 on gun 2
const uint8_t MAX_PLAYERS = 2;         // number of players in the game

const int DEFAULT_HEALTH = 10;         // the PC can change it with <S,n>
const int DAMAGE_PER_HIT = 1;          // health lost for every hit
const int LOW_HEALTH = 3;              // green LED blinks at or below this

const int MAGAZINE_SIZE = 6;           // blasts per magazine
const unsigned long RELOAD_MIN_MS = 2000;   // magazine must stay out this long
const bool RELOAD_SENSOR_FITTED = true; //to inform the prgoram that there is a a magzine 

// Level on the sensor OUT pin when the magazine (white patch) is in front
// of it. TCRT5000 modules give LOW when they see a reflection (D1 LED on).
// Test4_Reload tells you which level your module gives.
const int MAGAZINE_IN_LEVEL = LOW;

const uint8_t IR_POWER_PERCENT = 33;   
const uint8_t COPIES_PER_SHOT = 3;     // each blast is sent 3 times so at least one gets through
const unsigned long FIRE_COOLDOWN_MS = 250;   // minimum time between two blasts
const unsigned long STATUS_PERIOD_MS = 1000;  // send the status to the PC every second

const bool BUZZER_ACTIVE_LOW = true;   // our module beeps when I/O is LOW

// =====================================================================
// PINS
// =====================================================================
const uint8_t PIN_IR_RECEIVE = 2;      // KY-022 IR receiver signal (chest)
const uint8_t PIN_BUTTON = 4;          // trigger button
const uint8_t PIN_VIBE = 5;            // vibration motor
const uint8_t PIN_LED_GREEN = 6;       // green LED
const uint8_t PIN_LED_RED = 7;         // red LED
const uint8_t PIN_BUZZER = 8;          // active buzzer
const uint8_t PIN_IR_SEND = 9;         // Timer1 output OC1A, must be D9
const uint8_t PIN_XBEE_RX = 10;        // Uno receives on D10 (from XBee tx)
const uint8_t PIN_XBEE_TX = 11;        // Uno sends on D11 (to XBee rx)
const uint8_t PIN_RELOAD = 12;         // reload sensor OUT

// =====================================================================
// FEEDBACK DEVICES  
// Every device gets the same calls, PlayerNode does not need to know
// which device is which. update() runs timed effects without delay().
// =====================================================================
class FeedbackDevice {
public:
  // "= 0" means every device MUST write its own version of these
  virtual void begin() = 0;
  virtual void playHit() = 0;
  virtual void showHealth(int health, int maxHealth) = 0;
  virtual void update() = 0;
  // "{}" means a device can ignore these if they don't apply to it
  virtual void playFire() {}
  virtual void playEmpty() {}                    // trigger pulled, no ammo
  virtual void showReloading(bool reloading) {}  // magazine out / back in
  virtual void playReloadDone() {}
  virtual void playStart() {}
  virtual void playGameOver(bool won) {}
};

// ---------- green + red LED ----------
class StatusLed : public FeedbackDevice {
  uint8_t greenPin, redPin;                  // the two LED pins
  int health = 1, maxHealth = 1;             // ignore: for tests
  bool won = false, gameOver = false, reloading = false;   // current game situation
  unsigned long flashUntil = 0;              // time (ms) until which the red flash stays on
public:
  // constructor: remember which pins the LEDs are on
  StatusLed(uint8_t g, uint8_t r) : greenPin(g), redPin(r) {}

  // set both LED pins as outputs
  void begin() override {
    pinMode(greenPin, OUTPUT);
    pinMode(redPin, OUTPUT);
  }
  void playHit() override { flashUntil = millis() + 300; }   // red flash
  void playEmpty() override { flashUntil = millis() + 120; } // short red blink
  void showReloading(bool r) override { reloading = r; }     // remember if we are reloading
  void showHealth(int h, int m) override { health = h; maxHealth = m; }   // remember the health
  void playStart() override { gameOver = false; }            // new match: game not over
  void playGameOver(bool w) override { gameOver = true; won = w; }   // remember if we won

  // called every loop: decides what the two LEDs show right now
  void update() override {
    unsigned long now = millis();            // current time in ms
    bool green, red;                         // what each LED should be (on / off)
    if (health <= 0) {                       // dead
      green = false;
      red = true;
    } else if ((long)(now - flashUntil) < 0) {   // just got hit / empty click
      green = false;
      red = true;
    } else if (gameOver && won) {            // winner: fast green blink
      green = (now / 150) % 2;               // flips on/off every 150 ms
      red = false;
    } else if (reloading) {                  // magazine out: fast green blink
      green = (now / 100) % 2;               // flips on/off every 100 ms
      red = false;
    } else if (health <= LOW_HEALTH) {       // low health: slow green blink
      green = (now / 400) % 2;               // flips on/off every 400 ms
      red = false;
    } else {                                 // alive
      green = true;
      red = false;
    }
    digitalWrite(greenPin, green);           // turn the green LED on/off
    digitalWrite(redPin, red);               // turn the red LED on/off
  }
};

// ---------- active buzzer ----------
// A sound is a list of times in ms: on, off, on, off ...
const uint16_t SOUND_FIRE[] = {40};                  // one short beep
const uint16_t SOUND_EMPTY[] = {15};               // dry "click"
const uint16_t SOUND_RELOAD[] = {50, 50, 50, 50, 120};   // beep-beep-beeep
const uint16_t SOUND_HIT[] = {80, 60, 80};           // double beep
const uint16_t SOUND_DEAD[] = {1200};                // one long buzz
const uint16_t SOUND_START[] = {100, 100, 100, 100, 400};   // beep beep beeeep
const uint16_t SOUND_WIN[] = {80, 60, 80, 60, 80, 60, 600}; // victory tune
const uint16_t SOUND_LOSE[] = {600, 200, 600};       // two long buzzes

class Buzzer : public FeedbackDevice {
  uint8_t pin;                               // buzzer pin
  const uint16_t* sound = nullptr;           // the sound being played (nullptr = silent)
  uint8_t length = 0, step = 0;              // how many steps the sound has, and which one we are on
  unsigned long stepEnd = 0;                 // time (ms) when the current step ends
  int lastHealth = -1;                       // to notice the moment the player dies

  // turn the buzzer on or off (handles our "LOW = beep" module)
  void output(bool on) {
    digitalWrite(pin, (on != BUZZER_ACTIVE_LOW) ? HIGH : LOW);
  }
  // start playing a sound from its first step
  void play(const uint16_t* s, uint8_t len) {
    sound = s;
    length = len;
    step = 0;
    stepEnd = millis() + s[0];               // first step lasts s[0] ms
    output(true);                            // step 0 is always "on"
  }
public:
  // constructor: remember the buzzer pin
  Buzzer(uint8_t p) : pin(p) {}

  void begin() override {
    output(false);                           // silent before the pin becomes an output
    pinMode(pin, OUTPUT);
    output(false);
  }
  // each game event plays its own sound (the number = how many steps it has)
  void playHit() override { play(SOUND_HIT, 3); }
  void playFire() override { play(SOUND_FIRE, 1); }
  void playEmpty() override { play(SOUND_EMPTY, 1); }
  void playReloadDone() override { play(SOUND_RELOAD, 5); }
  void playStart() override { play(SOUND_START, 5); }
  void playGameOver(bool won) override {
    if (won) play(SOUND_WIN, 7);             // winner tune
    else play(SOUND_LOSE, 3);                // loser tune
  }
  void showHealth(int h, int m) override {
    if (h <= 0 && lastHealth > 0) play(SOUND_DEAD, 1);   // just died
    lastHealth = h;
  }
  // called every loop: moves to the next step of the sound when its time is up
  void update() override {
    if (sound == nullptr || (long)(millis() - stepEnd) < 0) return;   // nothing playing, or step not over yet
    step++;                                  // go to the next step
    if (step >= length) {                    // sound finished
      sound = nullptr;
      output(false);
      return;
    }
    stepEnd = millis() + sound[step];        // how long this new step lasts
    output(step % 2 == 0);                   // even steps = on, odd = off
  }
};

// ---------- vibration motor (LilyPad Vibe Board) ----------
class VibrationMotor : public FeedbackDevice {
  uint8_t pin;                               // motor pin
  unsigned long offAt = 0;                   // time (ms) to switch the motor off (0 = not running)
  int lastHealth = -1;                       // to notice the moment the player dies
public:
  // constructor: remember the motor pin
  VibrationMotor(uint8_t p) : pin(p) {}

  // set the pin as an output and make sure the motor starts off
  void begin() override {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  // hit: motor on for 0.4 s
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
  // called every loop: switch the motor off when its time is up
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
  uint8_t pin;                               // the pin the sensor is on
public:
  // constructor: remember the pin
  Sensor(uint8_t p) : pin(p) {}
  virtual void begin() = 0;                  // set up the sensor
  virtual bool isTriggered() = 0;            // true once per new event
};

// ---------- trigger button module ----------
class PushButton : public Sensor {
  int idleLevel = LOW;                       // pin level when the button is NOT pressed
  bool lastPressed = false;                  // was it pressed last time we checked
  unsigned long lastChange = 0;              // time of the last change (for debounce)
public:
  // constructor: pass the pin to the Sensor parent class
  PushButton(uint8_t p) : Sensor(p) {}

  void begin() override {
    pinMode(pin, INPUT);                     // button is an input
    delay(50);                               // let the pin settle
    idleLevel = digitalRead(pin);            // reads its level while the button is not being pressed
  }
  // true once for each press (with 30 ms debounce)
  bool isTriggered() override {
    bool pressed = digitalRead(pin) != idleLevel;   // different from idle = pressed
    if (pressed == lastPressed || millis() - lastChange < 30) return false;   // no change, or too soon (bounce)
    lastPressed = pressed;
    lastChange = millis();
    return pressed;                          // true only on the press, not on the release
  }
};

// ---------- reload sensor: TCRT5000 reflective IR module ----------
// Looks into the magazine slot. Magazine in = its white patch reflects
// the sensor's IR back = "detected". The reading is debounced for 50 ms
// so a wobbling magazine doesn't flicker.
class PhotoelectricSensor : public Sensor {
  bool magazineIn = true;                    // confirmed state of the magazine
  bool lastRaw = true;                       // last direct reading
  unsigned long rawSince = 0;                // time the direct reading last changed
  bool changed = false;
public:
  // constructor: pass the pin to the Sensor parent class
  PhotoelectricSensor(uint8_t p) : Sensor(p) {}

  void begin() override {
    pinMode(pin, INPUT);                     // sensor output is an input
    delay(50);                               // let the pin settle
    magazineIn = lastRaw = readRaw();        // start with the current magazine state
    rawSince = millis();
  }
  // direct reading: true = magazine in
  bool readRaw() {
    if (!RELOAD_SENSOR_FITTED) return virtualIn;    // no sensor: use the fake one(for testing purposes only)
    return digitalRead(pin) == MAGAZINE_IN_LEVEL;   // sensor level = "magazine in" level
  }
  bool isMagazineIn() { return magazineIn; }       // confirmed magazine state
  bool virtualIn = true;                   // used by the 'r' key when no sensor is fitted

  // true once each time the magazine goes out or comes in
  bool isTriggered() override {
    bool raw = readRaw();
    if (raw != lastRaw) {                    // reading just changed: restart the 50 ms timer
      lastRaw = raw;
      rawSince = millis();
    }
    if (raw != magazineIn && millis() - rawSince >= 50) {   // new reading stayed the same for 50 ms
      magazineIn = raw;                      // accept it
      return true;
    }
    return false;
  }
};

// ---------- IR receiver KY-022 ----------
class IRReceiver : public Sensor {
  uint8_t ownId;                             // this gun's player ID
  uint8_t lastShotFrom[MAX_PLAYERS + 1];     // last shot number counted from each player
  uint8_t shooter = 0;                       // who fired the last counted hit
  unsigned int noiseFrames = 0;              // frames that were not real shots (for testing)
public:
  // constructor: remember the pin and our own ID
  IRReceiver(uint8_t p, uint8_t id) : Sensor(p), ownId(id) {}

  void begin() override {
    IrReceiver.begin(pin, DISABLE_LED_FEEDBACK);   // start the IRremote receiver on this pin
    clearHistory();
  }
  // forget all old shot numbers (used at the start of a match)
  void clearHistory() {
    for (uint8_t i = 0; i <= MAX_PLAYERS; i++) lastShotFrom[i] = 0;
  }
  bool isIdle() { return IrReceiver.isIdle(); }   // true when no frame is arriving right now
  uint8_t getShooter() { return shooter; }
  unsigned int getNoiseFrames() { return noiseFrames; }//for test

  // true if a NEW blast from the other player arrived
  bool isTriggered() override {
    if (!IrReceiver.decode()) return false;  // no complete frame received yet

    // read what the frame contains
    decode_type_t protocol = IrReceiver.decodedIRData.protocol;   // type of IR code
    uint16_t address = IrReceiver.decodedIRData.address;          // shooter's player ID
    uint16_t command = IrReceiver.decodedIRData.command;          // shot number
    bool overflow = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW;   // frame was broken
    IrReceiver.resume();                     // get ready for the next frame

    // NEC2 = an NEC frame less than 70 ms after the last one (our copies 2 and 3)
    bool ours = (protocol == NEC || protocol == NEC2) && !overflow &&
                address >= 1 && address <= MAX_PLAYERS &&
                command >= 1 && command <= 200;
    if (!ours) {                             // not one of our shots: just noise
      noiseFrames++;
      return false;
    }
    if (address == ownId) return false;                 // my own blast bounced back
    if (command == lastShotFrom[address]) return false; // copy of a blast already counted

    lastShotFrom[address] = command;         // remember this shot so its copies are ignored
    shooter = address;                       // remember who hit us
    return true;                             // it's a real hit
  }
};

// =====================================================================
// IR EMITTER  (TSAL6100 driven by the BC107B transistor on D9)
// Timer1 makes the 38 kHz carrier in hardware, so receiver interrupts
// can't disturb it. The duty cycle sets the power.
// =====================================================================
class IREmitter {
  unsigned long edgeTime = 0;                // time (us) when the current pulse should end

  // start the 38 kHz signal on D9 (IR LED flashing)
  void carrierOn() {
    TCNT1 = 0;                               // restart the timer count
    TCCR1A |= _BV(COM1A1);                   // connect Timer1 to D9
  }
  // stop the 38 kHz signal (IR LED off)
  void carrierOff() {
    TCCR1A &= ~(_BV(COM1A1) | _BV(COM1A0));  // disconnect, D9 back to normal pin
    digitalWrite(PIN_IR_SEND, LOW);
  }
  // wait until the given time (in microseconds)
  void waitUntil(unsigned long t) {
    while ((long)(micros() - t) < 0) {}
  }
  // "mark" = IR on for us microseconds, "space" = IR off for us microseconds
  void mark(unsigned int us) { carrierOn(); edgeTime += us; waitUntil(edgeTime); }
  void space(unsigned int us) { carrierOff(); edgeTime += us; waitUntil(edgeTime); }
  // send one byte, lowest bit first: short space = 0, long space = 1
  void sendByte(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++) {
      mark(560);
      space((b & 1) ? 1690 : 560);
      b >>= 1;                               // move to the next bit
    }
  }
public:
  // set up Timer1 to make the 38 kHz signal with the chosen power
  void begin(uint8_t powerPercent) {
    pinMode(PIN_IR_SEND, OUTPUT);
    digitalWrite(PIN_IR_SEND, LOW);          // IR LED off at start
    // Timer1 fast PWM mode 14: TOP = ICR1, no prescaler
    TCCR1A = _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
    ICR1 = (F_CPU / 38000UL) - 1;            // 420 on a 16 MHz Uno -> 38 kHz
    OCR1A = ((uint32_t)(ICR1 + 1) * powerPercent) / 100;   // how long the LED is on in each cycle = power
  }
  // one NEC frame: about 68 ms
  void sendNEC(uint8_t address, uint8_t command) {
    edgeTime = micros();                     // start timing now
    mark(9000);                              // start pulse: 9 ms on
    space(4500);                             // then 4.5 ms off
    sendByte(address);                       // player ID
    sendByte(~address);                      // player ID inverted (error check)
    sendByte(command);                       // shot number
    sendByte(~command);                      // shot number inverted (error check)
    mark(560);                               // end pulse
    carrierOff();
  }
};



// =====================================================================
// XBEE RADIO  (LilyPad XBee board on SoftwareSerial D10/D11)
// =====================================================================
class XBeeRadio {
  SoftwareSerial port;                       // serial link to the XBee
  char buffer[16];                           // holds an incoming command
  uint8_t length = 0;                        // how many characters are in the buffer
  bool inMessage = false;                    // true while we are between '<' and '>'
public:
  char command = 0;                          // last command letter received ('S' or 'W')
  int value = 0;                             // the number after it

  // constructor: tell SoftwareSerial which pins to use
  XBeeRadio(uint8_t rx, uint8_t tx) : port(rx, tx) {}
  void begin() { port.begin(9600); }         // same speed as the XBee

  // build and send the status line, e.g. <1,12,3,7,4,1,2,30>
  void sendStatus(uint8_t id, int shots, int hits, int health, int ammo, bool magIn, int reloads) {
    char line[48];
    int mag = magIn ? 1 : 0;                 // magazine in = 1, out = 0
    snprintf(line, sizeof(line), "<%d,%d,%d,%d,%d,%d,%d,%d>", id, shots, hits, health, ammo, mag,
             reloads, id + shots + hits + health + ammo + mag + reloads);   // last number = check sum
    port.println(line);                      // send it to the XBee
  }

  // reads what arrived; true when a full <X,n> command is ready in command/value
  bool readCommand() {
    while (port.available()) {               // for every character waiting
      char c = port.read();
      if (c == '<') {                        // start of a command
        inMessage = true;
        length = 0;
      } else if (c == '>' && inMessage) {    // end of a command
        inMessage = false;
        buffer[length] = 0;                  // end the text
        command = buffer[0];                 // first letter = the command
        value = (length > 2 && buffer[1] == ',') ? atoi(buffer + 2) : 0;   // number after the comma
        if (command == 'S' || command == 'W') return true;   // only accept known commands
      } else if (inMessage) {                // a character inside the command
        if (length < sizeof(buffer) - 1) buffer[length++] = c;
        else inMessage = false;              // too long, junk
      }
    }
    return false;                            // no complete command yet
  }
};

// =====================================================================
// PLAYER NODE  (main class: game state + coordinates everything)
// =====================================================================
class PlayerNode {
  // the parts of the gun this class controls
  uint8_t id;
  PushButton& trigger;
  PhotoelectricSensor& magazine;
  IRReceiver& receiver;
  IREmitter& emitter;
  XBeeRadio& radio;
  FeedbackDevice** devices;                  // list of feedback devices (LED, buzzer, motor)
  uint8_t deviceCount;

  // the game state of this player
  int maxHealth = DEFAULT_HEALTH;
  int health = DEFAULT_HEALTH;
  int shots = 0;
  int hitsTaken = 0;
  int ammo = MAGAZINE_SIZE;
  int reloads = 0;
  bool reloading = false;                  // magazine is out
  unsigned long magazineOutSince = 0;      // when the magazine was pulled out
  uint8_t shotNumber = 1;                  // number put in each blast, 1..200
  unsigned long lastShot = 0;              // time of the last blast (for the cooldown)
  unsigned long lastStatus = 0;            // time the last status was sent
  bool statusDue = true;                   // true = something changed, send the status now

  // tell every feedback device the current health
  void showHealthOnAll() {
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->showHealth(health, maxHealth);
  }
  // let every feedback device run its timed effects
  void updateDevices() {
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->update();
  }

  // look at the receiver; a frame from the other player is a hit
  void checkReceiver() {
    if (receiver.isTriggered()) takeHit(receiver.getShooter());
  }

public:
  // constructor: connect the player to all its parts
  PlayerNode(uint8_t playerId, PushButton& b, PhotoelectricSensor& m, IRReceiver& r, IREmitter& e,
             XBeeRadio& x, FeedbackDevice** d, uint8_t n)
    : id(playerId), trigger(b), magazine(m), receiver(r), emitter(e), radio(x), devices(d), deviceCount(n) {}

  bool isAlive() { return health > 0; }

  // set up every part of the gun, then start a match
  void begin() {
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->begin();
    trigger.begin();
    magazine.begin();
    receiver.begin();
    emitter.begin(IR_POWER_PERCENT);
    radio.begin();

    randomSeed(analogRead(A0) ^ micros());   // random start from an unconnected pin
    shotNumber = random(1, 201);             // new numbers after a reset, so old ones never clash

    startMatch(DEFAULT_HEALTH);
    // welcome message in the Serial Monitor
    Serial.print(F("LASER ROYALE gun, player "));
    Serial.println(id);
    Serial.println(F("USB commands: f = fire, s = status, n = new match, r = fake magazine out/in"));
    if (!magazine.isMagazineIn()) Serial.println(F("Magazine is OUT: insert it to fire"));
  }

  // reset everything for a new match
  void startMatch(int newHealth) {
    if (newHealth < 1 || newHealth > 99) newHealth = DEFAULT_HEALTH;   // ignore strange values
    maxHealth = newHealth;
    health = newHealth;
    shots = 0;
    hitsTaken = 0;
    ammo = MAGAZINE_SIZE;
    reloads = 0;
    reloading = !magazine.isMagazineIn();
    magazineOutSince = millis();
    receiver.clearHistory();                 // forget old shot numbers
    for (uint8_t i = 0; i < deviceCount; i++) {
      devices[i]->playStart();               // start sound
      devices[i]->showReloading(reloading);
    }
    showHealthOnAll();
    statusDue = true;                        // tell the PC right away
    Serial.print(F("NEW MATCH, health "));
    Serial.println(health);
  }

  // magazine pulled out or pushed back in
  void magazineChanged() {
    if (!magazine.isMagazineIn()) {          // pulled out: reload starts
      reloading = true;
      magazineOutSince = millis();           // start counting the reload time
      Serial.println(F("MAGAZINE OUT, reloading..."));
    } else {                                 // pushed back in
      reloading = false;
      if (millis() - magazineOutSince >= RELOAD_MIN_MS) {   // was out long enough
        ammo = MAGAZINE_SIZE;                // full magazine again
        reloads++;
        for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playReloadDone();
        Serial.print(F("RELOADED, ammo "));
        Serial.println(ammo);
      } else {
        Serial.println(F("Magazine back too early, not refilled"));
      }
    }
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->showReloading(reloading);   // update the LEDs
    statusDue = true;                        // tell the PC
  }

  // the trigger was pressed
  void fire() {
    if (!isAlive()) {                        // dead players can't shoot
      Serial.println(F("Dead, can't fire"));
      return;
    }
    if (millis() - lastShot < FIRE_COOLDOWN_MS) return;   // too soon after the last shot
    lastShot = millis();
    if (reloading || ammo <= 0) {            // no magazine / empty: click
      for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playEmpty();
      Serial.println(reloading ? F("click (reloading)") : F("click (empty, reload!)"));
      return;
    }
    ammo--;                                  // use one shot
    shots++;
    shotNumber = (shotNumber >= 200) ? 1 : shotNumber + 1;   // next shot number, back to 1 after 200

    // ~270 ms: 3 copies, checking the receiver between them so an
    // incoming blast is not lost while we shoot
    for (uint8_t c = 0; c < COPIES_PER_SHOT; c++) {
      checkReceiver();
      emitter.sendNEC(id, shotNumber);       // send the blast: my ID + shot number
      delay(12);                             // receiver needs 8 ms of quiet to end a frame
      checkReceiver();
    }
    for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playFire();   // fire sound
    statusDue = true;                        // tell the PC
    Serial.print(F("FIRE  shot "));
    Serial.print(shots);
    Serial.print(F("  ammo left "));
    Serial.println(ammo);
  }

  // we were hit by the other player
  void takeHit(uint8_t shooter) {
    if (!isAlive()) return;                  // already dead
    hitsTaken++;
    health -= DAMAGE_PER_HIT;
    if (health < 0) health = 0;              // health can't go below 0
    if (health > 0) {                        // still alive: hit feedback
      for (uint8_t i = 0; i < deviceCount; i++) devices[i]->playHit();
    }
    showHealthOnAll();                       // devices react (and play the death effects at 0)
    statusDue = true;                        // tell the PC
    Serial.print(F("HIT by player "));
    Serial.print(shooter);
    Serial.print(F("  health "));
    Serial.println(health);
    if (!isAlive()) Serial.println(F("*** DEAD ***"));
  }

  // print all the game numbers in the Serial Monitor (for testing)
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

  // runs over and over: checks every input and updates every output
  void loop() {
    if (trigger.isTriggered()) fire();               // trigger pressed: shoot
    if (magazine.isTriggered()) magazineChanged();   // magazine moved: reload logic
    checkReceiver();                                 // were we hit?

    // commands from the PC
    if (radio.readCommand()) {
      if (radio.command == 'S') {            // <S,n> = start a new match
        startMatch(radio.value);
      } else if (radio.command == 'W') {     // <W,id> = match over, id won
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

    updateDevices();                         // LEDs, buzzer and motor do their timed effects
  }
};

// =====================================================================
// OBJECTS
// =====================================================================
// the three feedback devices, collected in one list
StatusLed statusLed(PIN_LED_GREEN, PIN_LED_RED);
Buzzer buzzer(PIN_BUZZER);
VibrationMotor vibration(PIN_VIBE);
FeedbackDevice* feedback[] = {&statusLed, &buzzer, &vibration};

// the sensors, the IR emitter and the radio
PushButton trigger(PIN_BUTTON);
PhotoelectricSensor magazineSensor(PIN_RELOAD);
IRReceiver irReceiver(PIN_IR_RECEIVE, PLAYER_ID);
IREmitter irEmitter;
XBeeRadio xbee(PIN_XBEE_RX, PIN_XBEE_TX);

// the player, connected to all the parts above
PlayerNode player(PLAYER_ID, trigger, magazineSensor, irReceiver, irEmitter, xbee, feedback, 3);

// runs once when the Uno starts
void setup() {
  Serial.begin(9600);                        // USB Serial Monitor at 9600 baud
  player.begin();                            // set up the gun
}

// runs over and over forever
void loop() {
  player.loop();                             // one round of checking and updating
}
