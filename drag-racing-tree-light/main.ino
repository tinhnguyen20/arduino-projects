/*
  Reaction Timer
  Arduino Uno + KYX-3461BS 4-digit display + 5-LED light tree + 1 button

  How it plays
    1. Blue is lit: press the button to start.
    2. The three clear lights come on one at a time.
    3. After a random 1-3 second pause, green comes on. Press as fast as you can.
    4. Time (ms) shows on the display (it flashes on a new best).
       Press once to get back to the start.
    Pressing before green is a false start: blue flashes and "Err" shows.
    While waiting to start, the display scrolls "rEAction trEE tiMEr",
    "PrESS to StArt" and, once you have one, your best time.

  Wiring
    Light tree   A0 blue, A1-A3 clear, A4 green (each LED with a 330 ohm resistor)
    Button       A5 to GND (uses the Uno's internal pull-up, no resistor needed)
    Display      segments A-G on D2-D8, each through a 1 kOhm resistor
                 digit 1-4 (left to right) on D10-D13, no resistors
*/

// The KYX-3461BS is common anode. If your display stays blank or shows
// scrambled segments, change this to false.
const bool COMMON_ANODE = true;

// Pins
const byte SEG_PINS[7]   = {2, 3, 4, 5, 6, 7, 8};  // segments A, B, C, D, E, F, G
const byte DIGIT_PINS[4] = {10, 11, 12, 13};       // digit 1 (left) to digit 4 (right)

const byte LED_BLUE     = A0;
const byte LED_CLEAR[3] = {A1, A2, A3};
const byte LED_GREEN    = A4;
const byte BUTTON       = A5;

// Timing (milliseconds)
const unsigned long STEP_MS     = 500;   // gap between countdown lights
const unsigned long WAIT_MIN_MS = 1000;  // shortest pause before green
const unsigned long WAIT_MAX_MS = 3000;  // longest pause before green
const unsigned long TIMEOUT_MS  = 9999;  // give up if nobody presses
const unsigned long DEBOUNCE_MS = 40;    // ignore button bounce for this long
const unsigned long SCROLL_MS   = 300;   // how fast the message scrolls (smaller = faster)

//  The scrolling message 
// Change these to say whatever you like. Letters, numbers and spaces work.
// Some letters (k, v, w, x, z) can't be drawn on a 7-segment display and show as blanks.
const char MSG_TITLE[] = "reaction tree timer";
const char MSG_PRESS[] = "press to start";

//  Characters (bit 0 = segment A ... bit 6 = segment G) 
const byte NUM[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
const byte CH_BLANK = 0x00, CH_DASH = 0x40;
const byte CH_E = 0x79, CH_r = 0x50;

char scrollText[80];  // the full message, built in enterReady()
int scrollLen = 0;

byte frame[4];   // what each digit is showing right now

enum State { READY, COUNTDOWN, WAITING, GO, RESULT, FALSE_START };
State state;
unsigned long stateStart;    // millis() when the current state began
unsigned long waitMs;        // random pause for this round
unsigned long goMicros;      // micros() when green came on
unsigned long lastTime = 0;  // last reaction time in ms (0 = timed out)
unsigned long bestTime = 0;  // best reaction time in ms (0 = none yet)
bool newBest = false;
bool seeded = false;

// Function declarations (lets the code below appear in any order)
void setState(State s);
void enterReady();
void falseStart();
void allLightsOff();
bool buttonPressed();
void showWord(byte d1, byte d2, byte d3, byte d4);
void showNumber(unsigned long n);
void refreshDisplay();
byte charToSegments(char c);

void setup() {
  for (byte i = 0; i < 7; i++) pinMode(SEG_PINS[i], OUTPUT);
  for (byte i = 0; i < 4; i++) {
    pinMode(DIGIT_PINS[i], OUTPUT);
    digitalWrite(DIGIT_PINS[i], COMMON_ANODE ? LOW : HIGH);  // all digits off
  }
  pinMode(LED_BLUE, OUTPUT);
  for (byte i = 0; i < 3; i++) pinMode(LED_CLEAR[i], OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  enterReady();
}

void loop() {
  refreshDisplay();  // must run constantly: the display is multiplexed

  bool pressed = buttonPressed();
  unsigned long pressMicros = micros();
  unsigned long elapsed = millis() - stateStart;

  switch (state) {

    case READY:
      {
        // Slide the message one letter to the left every SCROLL_MS.
        int pos = (elapsed / SCROLL_MS) % scrollLen;
        for (byte i = 0; i < 4; i++) {
          frame[i] = charToSegments(scrollText[(pos + i) % scrollLen]);
        }
      }
      if (pressed) {
        if (!seeded) {  // human timing makes a good random seed
          randomSeed(pressMicros);
          seeded = true;
        }
        allLightsOff();
        digitalWrite(LED_BLUE, HIGH);
        showWord(CH_DASH, CH_DASH, CH_DASH, CH_DASH);
        setState(COUNTDOWN);
      }
      break;

    case COUNTDOWN:
      if (pressed) { falseStart(); break; }
      for (byte i = 0; i < 3; i++) {
        digitalWrite(LED_CLEAR[i], elapsed >= (i + 1) * STEP_MS ? HIGH : LOW);
      }
      if (elapsed >= 3 * STEP_MS) {
        waitMs = random(WAIT_MIN_MS, WAIT_MAX_MS + 1);
        setState(WAITING);
      }
      break;

    case WAITING:
      if (pressed) { falseStart(); break; }
      if (elapsed >= waitMs) {
        allLightsOff();
        digitalWrite(LED_GREEN, HIGH);
        goMicros = micros();
        setState(GO);
      }
      break;

    case GO:
      if (pressed) {
        lastTime = (pressMicros - goMicros + 500) / 1000;  // round to nearest ms
        if (lastTime == 0) lastTime = 1;
        newBest = (bestTime == 0 || lastTime < bestTime);
        if (newBest) bestTime = lastTime;
        showNumber(lastTime);
        setState(RESULT);
      } else if (elapsed > TIMEOUT_MS) {
        lastTime = 0;
        newBest = false;
        digitalWrite(LED_GREEN, LOW);
        showWord(CH_DASH, CH_DASH, CH_DASH, CH_DASH);
        setState(RESULT);
      }
      break;

    case RESULT:
      if (newBest && elapsed < 2000) {
        if ((elapsed / 200) % 2) showWord(CH_BLANK, CH_BLANK, CH_BLANK, CH_BLANK);
        else showNumber(lastTime);
      } else if (lastTime > 0) {
        showNumber(lastTime);
      }
      if (pressed && elapsed > 300) enterReady();
      break;

    case FALSE_START:
      digitalWrite(LED_BLUE, (elapsed / 150) % 2 == 0 ? HIGH : LOW);
      if (pressed && elapsed > 300) enterReady();
      break;
  }
}

//  Game helpers 

void setState(State s) {
  state = s;
  stateStart = millis();
}

void enterReady() {
  allLightsOff();
  digitalWrite(LED_BLUE, HIGH);
  // Build the message. The spaces make a gap before it repeats.
  if (bestTime > 0) {
    snprintf(scrollText, sizeof(scrollText), "    %s    %s    best %lu    ",
             MSG_TITLE, MSG_PRESS, bestTime);
  } else {
    snprintf(scrollText, sizeof(scrollText), "    %s    %s    ", MSG_TITLE, MSG_PRESS);
  }
  scrollLen = strlen(scrollText);
  setState(READY);
}

void falseStart() {
  allLightsOff();
  showWord(CH_BLANK, CH_E, CH_r, CH_r);
  setState(FALSE_START);
}

void allLightsOff() {
  digitalWrite(LED_BLUE, LOW);
  for (byte i = 0; i < 3; i++) digitalWrite(LED_CLEAR[i], LOW);
  digitalWrite(LED_GREEN, LOW);
}

//  Button 
// Returns true once per press. It reacts on the very first contact, so it
// adds no delay to your time, then ignores the button for DEBOUNCE_MS so
// contact bounce isn't counted as extra presses.
bool buttonPressed() {
  static bool lastState = HIGH;
  static unsigned long lockedAt = 0;
  if (millis() - lockedAt < DEBOUNCE_MS) return false;
  bool now = digitalRead(BUTTON);
  if (now != lastState) {
    lastState = now;
    lockedAt = millis();
    return now == LOW;  // LOW = pressed (pull-up)
  }
  return false;
}

// Display

// How each letter looks on a 7-segment digit (bit 0 = segment A ... bit 6 = G).
// Some letters have to borrow a shape: m looks like an upside-down U.
byte charToSegments(char c) {
  if (c >= '0' && c <= '9') return NUM[c - '0'];
  if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';  // treat capitals as lowercase
  switch (c) {
    case 'a': return 0x77;  case 'b': return 0x7C;  case 'c': return 0x58;
    case 'd': return 0x5E;  case 'e': return 0x79;  case 'f': return 0x71;
    case 'g': return 0x6F;  case 'h': return 0x74;  case 'i': return 0x04;
    case 'j': return 0x1E;  case 'l': return 0x38;  case 'm': return 0x37;
    case 'n': return 0x54;  case 'o': return 0x5C;  case 'p': return 0x73;
    case 'q': return 0x67;  case 'r': return 0x50;  case 's': return 0x6D;
    case 't': return 0x78;  case 'u': return 0x1C;  case 'y': return 0x6E;
    case '-': return 0x40;  case '_': return 0x08;
    default:  return 0x00;  // space and anything we can't draw
  }
}

void showWord(byte d1, byte d2, byte d3, byte d4) {
  frame[0] = d1; frame[1] = d2; frame[2] = d3; frame[3] = d4;
}

// Right-aligned number without leading zeros, e.g. " 231".
void showNumber(unsigned long n) {
  if (n > 9999) n = 9999;
  for (int i = 3; i >= 0; i--) {
    if (n == 0 && i < 3) {
      frame[i] = CH_BLANK;
    } else {
      frame[i] = NUM[n % 10];
      n /= 10;
    }
  }
}

// Lights one digit at a time, switching every 2 ms. It happens fast enough
// that your eye sees all four digits lit at once.
void refreshDisplay() {
  static byte digit = 0;
  static unsigned long lastSwitch = 0;
  unsigned long now = micros();
  if (now - lastSwitch < 2000) return;
  lastSwitch = now;

  digitalWrite(DIGIT_PINS[digit], COMMON_ANODE ? LOW : HIGH);  // current digit off
  digit = (digit + 1) % 4;
  for (byte s = 0; s < 7; s++) {
    bool on = frame[digit] & (1 << s);
    // Common anode: a segment lights when its pin is LOW.
    digitalWrite(SEG_PINS[s], (on != COMMON_ANODE) ? HIGH : LOW);
  }
  digitalWrite(DIGIT_PINS[digit], COMMON_ANODE ? HIGH : LOW);  // next digit on
}
