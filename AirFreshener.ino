#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>
#include <EEPROM.h>

// ============================================================================
// Configuration (The "Map")
// ============================================================================
// Pin Assignments
const int servoPin = 9;         // Explicitly defined Pin 9 for the Servo
const int startButtonPin = 2;    // Physical pull-up button to start/reset the cycle
const int redLedPin = 6;        // RGB LED Red channel (PWM Pin 6)
const int greenLedPin = 5;      // RGB LED Green channel (PWM Pin 5)
const int blueLedPin = 3;       // RGB LED Blue channel (PWM Pin 3)

// Servo Angles & Timing
const int sprayAngle = 42;       // The "Squirt" actuation angle (degrees)
const int restAngle = 0;         // The "Home" resting position (degrees)
const unsigned int sprayHoldMs = 1000;  // Actuation hold duration (ms)
const unsigned int servoReturnMs = 500; // Delay to reach rest before detaching (ms)

// Hardware Options
const bool detachServoWhenIdle = true;  // Detach servo at rest to stop buzzing & save power
const bool commonAnodeLed = false;      // Set true if using common-anode RGB LED
const unsigned long canisterCapacity = 2400; // Estimated sprays per standard aerosol can

// EEPROM Storage Layout
const int EEPROM_CYCLE_START = 0;       // 4 bytes: unsigned long cycleStartUnix
const int EEPROM_SPRAY_COUNT = 4;       // 4 bytes: unsigned long totalSprayCount

// ============================================================================
// Hardware Instances & Global State
// ============================================================================
RTC_DS3231 rtc;
Servo sprayer;
uint32_t cycleStartUnix = 0;       // Saved cycle start timestamp
uint32_t totalSprayCount = 0;      // Total sprays used from canister

// Non-blocking button debouncing
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // ms

// Function Prototypes
void trigger(int count);
void runSchedule(float h);
void updateLedFeedback(float hoursIn, bool cycleActive);
void setRgb(uint8_t r, uint8_t g, uint8_t b);
void startNewCycle(uint32_t nowUnix);
void handleSerialCommands(uint32_t nowUnix, float hoursIn, bool cycleActive);
void printStatusReport(uint32_t nowUnix, float hoursIn, bool cycleActive);
void printHelp();

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println(F("\n========================================"));
  Serial.println(F(" Smart Air Freshener Initializing..."));
  Serial.println(F("========================================"));

  // Configure pins
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  setRgb(0, 0, 0);

  // Initialize servo safely
  sprayer.attach(servoPin);
  sprayer.write(restAngle);
  delay(300);
  if (detachServoWhenIdle) {
    sprayer.detach();
  }

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println(F("ERROR: DS3231 RTC not detected! Check SDA/SCL wiring."));
    while (1) {
      // Blink red warning indefinitely
      setRgb(255, 0, 0);
      delay(250);
      setRgb(0, 0, 0);
      delay(250);
    }
  }

  if (rtc.lostPower()) {
    Serial.println(F("NOTICE: RTC lost power. Setting fallback to build time."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Retrieve last saved start time from permanent memory
  EEPROM.get(EEPROM_CYCLE_START, cycleStartUnix);
  if (cycleStartUnix == 0xFFFFFFFF) {
    cycleStartUnix = 0; // Fresh uninitialized EEPROM
    EEPROM.put(EEPROM_CYCLE_START, cycleStartUnix);
  }

  // Retrieve total canister spray count
  EEPROM.get(EEPROM_SPRAY_COUNT, totalSprayCount);
  if (totalSprayCount == 0xFFFFFFFF) {
    totalSprayCount = 0; // Fresh uninitialized EEPROM
    EEPROM.put(EEPROM_SPRAY_COUNT, totalSprayCount);
  }

  Serial.print(F("Loaded Start Timestamp: "));
  Serial.println(cycleStartUnix);
  Serial.print(F("Canister Sprays Used: "));
  Serial.print(totalSprayCount);
  Serial.print(F(" / "));
  Serial.println(canisterCapacity);
  Serial.println(F("System Ready. Send 'G' or press button to start. '?' for help."));
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  DateTime now = rtc.now();
  uint32_t nowUnix = now.unixtime();

  // 1. Non-blocking button check with debouncing & edge detection
  int reading = digitalRead(startButtonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  bool cycleActive = (cycleStartUnix > 0);
  float hoursIn = 0.0;

  if (cycleActive) {
    uint32_t secondsElapsed = nowUnix - cycleStartUnix;
    uint32_t progressInCycle = secondsElapsed % 86400UL; // 24-hour cycle wrap
    hoursIn = progressInCycle / 3600.0;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    static int debouncedState = HIGH;
    if (reading != debouncedState) {
      debouncedState = reading;
      if (debouncedState == LOW) {
        // Physical button pressed (falling edge)
        startNewCycle(nowUnix);
        cycleActive = true;
        hoursIn = 0.0;
      }
    }
  }
  lastButtonState = reading;

  // 2. Process Serial / Bluetooth commands
  handleSerialCommands(nowUnix, hoursIn, cycleActive);

  // 3. Run the 24-hour logic if a cycle has been set (Active Window up to 8.05 hours)
  if (cycleActive) {
    if (hoursIn <= 8.05) {
      runSchedule(hoursIn);
    }
  }

  // 4. Update the Jeweling LED feedback
  updateLedFeedback(hoursIn, cycleActive);
}

// ============================================================================
// Cycle Management & Actuation
// ============================================================================
void startNewCycle(uint32_t nowUnix) {
  cycleStartUnix = nowUnix;
  EEPROM.put(EEPROM_CYCLE_START, cycleStartUnix);
  Serial.println(F(">>> Cycle Started Now! <<<"));

  // Quick visual confirmation (Cyan pulse)
  setRgb(0, 255, 255);
  delay(150);
  setRgb(0, 0, 0);
}

void runSchedule(float h) {
  static int lastSprayedInterval = -1;

  // Ankle Bone Sequence (Hour 1 to 2)
  if (h >= 1.1 && h < 1.15 && lastSprayedInterval != 1) { trigger(1); lastSprayedInterval = 1; }
  else if (h >= 1.2 && h < 1.25 && lastSprayedInterval != 2) { trigger(2); lastSprayedInterval = 2; }
  else if (h >= 1.5 && h < 1.55 && lastSprayedInterval != 3) { trigger(3); lastSprayedInterval = 3; }
  else if (h >= 2.0 && h < 2.05 && lastSprayedInterval != 4) { trigger(4); lastSprayedInterval = 4; }

  // General 2-hour maintenance intervals (Hours 4, 6, and 8)
  else if (h >= 4.0 && h < 4.05 && lastSprayedInterval != 5) { trigger(5); lastSprayedInterval = 5; }
  else if (h >= 6.0 && h < 6.05 && lastSprayedInterval != 6) { trigger(6); lastSprayedInterval = 6; }
  else if (h >= 8.0 && h < 8.05 && lastSprayedInterval != 7) { trigger(7); lastSprayedInterval = 7; }

  // Reset interval latch near the start of the daily cycle
  if (h < 0.1) {
    lastSprayedInterval = -1;
  }
}

void trigger(int count) {
  Serial.print(F("Spraying interval: "));
  Serial.println(count);

  // Visual feedback: bright white during mechanical spray
  setRgb(255, 255, 255);

  sprayer.attach(servoPin);
  sprayer.write(sprayAngle);
  delay(sprayHoldMs);

  sprayer.write(restAngle);
  delay(servoReturnMs);

  if (detachServoWhenIdle) {
    sprayer.detach();
  }

  // Increment spray counter and save to EEPROM
  totalSprayCount++;
  EEPROM.put(EEPROM_SPRAY_COUNT, totalSprayCount);

  Serial.print(F("Canister sprays used: "));
  Serial.print(totalSprayCount);
  if (totalSprayCount >= canisterCapacity) {
    Serial.println(F(" (WARNING: Canister near or past estimated capacity! Type 'C' after replacing)"));
  } else {
    Serial.println();
  }
}

// ============================================================================
// LED Feedback ("Jeweling")
// ============================================================================
void updateLedFeedback(float hoursIn, bool cycleActive) {
  // If cycle is not active or in Standby Mode (8.05h - 24.0h), turn LED off
  if (!cycleActive || hoursIn > 8.05) {
    setRgb(0, 0, 0);
    return;
  }

  // 0.0 - 1.0 Hours: Initialization -> Steady Blue
  if (hoursIn < 1.0) {
    setRgb(0, 0, 255);
    return;
  }

  // 1.0 - 2.05 Hours: Ankle Bone Sequence -> Flashing Red
  if (hoursIn < 2.05) {
    bool flashState = ((millis() / 250) % 2) == 0;
    if (flashState) {
      setRgb(255, 0, 0);
    } else {
      setRgb(0, 0, 0);
    }
    return;
  }

  // 2.05 - 8.05 Hours: Maintenance Window -> Green Pulse
  // Smooth breathing pulse between 20 and 255 brightness over a 2.5 second cycle
  unsigned long t = millis() % 2500;
  float angle = (t / 2500.0) * 3.14159265;
  int brightness = 20 + (int)(sin(angle) * 235.0);
  if (brightness < 0) brightness = 0;
  if (brightness > 255) brightness = 255;
  setRgb(0, (uint8_t)brightness, 0);
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  if (commonAnodeLed) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  analogWrite(redLedPin, r);
  analogWrite(greenLedPin, g);
  analogWrite(blueLedPin, b);
}

// ============================================================================
// Bluetooth & Serial Command Handler
// ============================================================================
void handleSerialCommands(uint32_t nowUnix, float hoursIn, bool cycleActive) {
  while (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '\r' || cmd == '\n' || cmd == ' ') continue;

    switch (cmd) {
      case 'G':
      case 'g':
      case 'S':
      case 's':
        startNewCycle(nowUnix);
        break;

      case 'T':
      case 't':
        Serial.println(F("Manual test spray triggered."));
        trigger(0); // 0 = manual test spray
        break;

      case 'P':
      case 'p':
        printStatusReport(nowUnix, hoursIn, cycleActive);
        break;

      case 'R':
      case 'r':
        cycleStartUnix = 0;
        EEPROM.put(EEPROM_CYCLE_START, cycleStartUnix);
        setRgb(0, 0, 0);
        Serial.println(F("Cycle stopped and reset to idle standby."));
        break;

      case 'C':
      case 'c':
        totalSprayCount = 0;
        EEPROM.put(EEPROM_SPRAY_COUNT, totalSprayCount);
        Serial.println(F("Canister spray counter reset to 0."));
        break;

      case '?':
      case 'H':
      case 'h':
        printHelp();
        break;

      default:
        Serial.print(F("Unknown command: '"));
        Serial.print(cmd);
        Serial.println(F("'. Send '?' for help."));
        break;
    }
  }
}

void printStatusReport(uint32_t nowUnix, float hoursIn, bool cycleActive) {
  Serial.println(F("\n--- Smart Air Freshener Status ---"));
  Serial.print(F("Current Unix Time: "));
  Serial.println(nowUnix);

  if (cycleActive) {
    Serial.println(F("Cycle Status: ACTIVE"));
    Serial.print(F("Hours into cycle: "));
    Serial.print(hoursIn, 2);
    Serial.println(F(" hrs"));

    if (hoursIn < 1.0) {
      Serial.println(F("Current Phase: Initialization (Steady Blue)"));
    } else if (hoursIn < 2.05) {
      Serial.println(F("Current Phase: Ankle Bone Sequence (Flashing Red)"));
    } else if (hoursIn <= 8.05) {
      Serial.println(F("Current Phase: Maintenance Window (Green Pulse)"));
    } else {
      Serial.println(F("Current Phase: Standby Mode (LED Off)"));
    }
  } else {
    Serial.println(F("Cycle Status: IDLE / NOT STARTED"));
  }

  Serial.print(F("Canister Sprays Used: "));
  Serial.print(totalSprayCount);
  Serial.print(F(" / "));
  Serial.println(canisterCapacity);

  if (totalSprayCount >= canisterCapacity) {
    Serial.println(F("WARNING: Canister is at or past estimated capacity!"));
  } else {
    Serial.print(F("Estimated sprays remaining: "));
    Serial.println(canisterCapacity - totalSprayCount);
  }
  Serial.println(F("----------------------------------"));
}

void printHelp() {
  Serial.println(F("\n--- Available Commands ---"));
  Serial.println(F("  G or S : Start / restart 24-hour cycle now"));
  Serial.println(F("  T      : Trigger manual test spray"));
  Serial.println(F("  P      : Print detailed system status"));
  Serial.println(F("  R      : Stop / reset active cycle to idle"));
  Serial.println(F("  C      : Reset canister spray counter to 0"));
  Serial.println(F("  ? or H : Print this help menu"));
  Serial.println(F("--------------------------"));
}
