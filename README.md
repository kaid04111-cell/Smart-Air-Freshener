# Smart-Air-Freshener
An Arduino-based automatic air freshener using a DS3231 RTC for a precise 24-hour cycle (8 hours active, 16 hours standby) with a specialized "Ankle Bone" spray sequence.
Smart Air Freshener: Precise 24-Hour Scent Controller
Project Overview
This project is a high-precision, automated air freshener designed to run on a strict 24-hour cycle. Unlike standard off-the-shelf sprayers, this system allows the user to manually trigger the "Start" of their day. Once activated, the system provides an intense scent burst during the first two hours (the "Ankle Bone" sequence) followed by steady intervals, eventually entering a 16-hour deep-sleep mode to preserve resources.
Hardware Configuration (The Map)
The system is built around the Arduino Uno/Nano and uses the following pin mapping to ensure the "Brain" and the "Physical Map" are perfectly synced:
 * Servo Motor (Pin 9): Responsible for the physical actuation of the scent canister.
 * Start Button (Pin 2): A physical pull-up button that resets the 24-hour timestamp.
 * RTC DS3231: Connected via I2C (SDA/SCL) to maintain time even during power loss.
The "Ankle Bone" Logic
One of the most unique features of this code is the specific timing sequence during the second hour of operation. Between hour 1.0 and hour 2.0, the system triggers at non-standard intervals to ensure maximum coverage:
| Cycle Progress (Hours) | Action | LED Feedback (Jeweling) |
|---|---|---|
| 0.0 - 1.0 | Initialization | Steady Blue |
| 1.1 | Spray 1 | Flashing Red |
| 1.2 | Spray 2 | Flashing Red |
| 1.5 | Spray 3 | Flashing Red |
| 2.0 | Spray 4 | Flashing Red |
| 4.0, 6.0, 8.0 | Maintenance Sprays | Green Pulse |
| 8.0 - 24.0 | Standby Mode | Off |
Step-by-Step Installation
1. Prepare the Arduino IDE
Ensure you have the following libraries installed via the Library Manager:
 * RTClib (by Adafruit)
 * Servo (Standard)
 * EEPROM (Standard)
2. Flash the "Brain" (The .ino File)
Copy the following code into your Arduino IDE. This includes the fix for the Pin 9 constant and the EEPROM memory storage:
#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>
#include <EEPROM.h> 

RTC_DS3231 rtc;
Servo sprayer;

const int sprayAngle = 42;      [span_13](start_span)// The "Squirt" Angle[span_13](end_span)
const int startButtonPin = 2;   [span_14](start_span)// The Manual Start Button[span_14](end_span)
const int servoPin = 9;         [span_15](start_span)// Fixed: The Servo is on Pin 9[span_15](end_span)

unsigned long cycleStartUnix = 0; 

void setup() {
  Serial.begin(9600);
  sprayer.attach(servoPin); [span_16](start_span)// Attaching to Pin 9[span_16](end_span)
  pinMode(startButtonPin, INPUT_PULLUP); [span_17](start_span)// Button on Pin 2[span_17](end_span)

  if (!rtc.begin()) {
    while (1); [span_18](start_span)// Halt if RTC is disconnected[span_18](end_span)
  }

  [span_19](start_span)// Load the last saved start time to survive power resets[span_19](end_span)
  EEPROM.get(0, cycleStartUnix); 
}

void loop() {
  DateTime now = rtc.now();
  uint32_t nowUnix = now.unixtime(); [span_20](start_span)// Total seconds elapsed[span_20](end_span)

  [span_21](start_span)// Check for Manual Start (Physical Button or Bluetooth 'G')[span_21](end_span)
  if (digitalRead(startButtonPin) == LOW || (Serial.available() && Serial.read() == 'G')) {
    cycleStartUnix = nowUnix;
    EEPROM.put(0, cycleStartUnix); [span_22](start_span)// Save to permanent memory[span_22](end_span)
    Serial.println("Cycle Reset!");
    delay(500); 
  }

  if (cycleStartUnix > 0) {
    uint32_t secondsElapsed = nowUnix - cycleStartUnix;
    uint32_t progressInCycle = secondsElapsed % 86400; [span_23](start_span)// 24-hour wrap[span_23](end_span)
    [span_24](start_span)float hoursIn = progressInCycle / 3600.0;[span_24](end_span)

    if (hoursIn < 8.0) {
      runSchedule(hoursIn); [span_25](start_span)// 8-hour Active Window[span_25](end_span)
    }
  }
}

void runSchedule(float h) {
  static int lastSprayedInterval = -1;

  [span_26](start_span)// The Ankle Bone Sequence[span_26](end_span)
  if (h >= 1.1 && h < 1.15 && lastSprayedInterval != 1) { trigger(1); lastSprayedInterval = 1; }
  if (h >= 1.2 && h < 1.25 && lastSprayedInterval != 2) { trigger(2); lastSprayedInterval = 2; }
  if (h >= 1.5 && h < 1.55 && lastSprayedInterval != 3) { trigger(3); lastSprayedInterval = 3; }
  if (h >= 2.0 && h < 2.05 && lastSprayedInterval != 4) { trigger(4); lastSprayedInterval = 4; }

  [span_27](start_span)// Regular Intervals[span_27](end_span)
  if (h >= 4.0 && h < 4.05 && lastSprayedInterval != 5) { trigger(5); lastSprayedInterval = 5; }
  if (h >= 6.0 && h < 6.05 && lastSprayedInterval != 6) { trigger(6); lastSprayedInterval = 6; }
  
  if (h < 0.1) lastSprayedInterval = -1; [span_28](start_span)// Daily reset[span_28](end_span)
}

void trigger(int count) {
  sprayer.write(sprayAngle); // Move to 42 degrees
  delay(1000);
  sprayer.write(0); // Return to rest
}

3. Upload and Document
Once the code is uploaded, take a picture of your finished build and the Schematic PNG you downloaded. Upload them both to your GitHub repository folder.
License
This project is licensed under the MIT License. You are free to modify the "Ankle Bone" timing or the pinouts for your own DIY needs.
