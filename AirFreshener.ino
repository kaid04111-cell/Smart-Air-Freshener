#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>
#include <EEPROM.h>

// --- Configuration (The "Map") ---
const int servoPin = 9;         [span_0](start_span)// Explicitly defined Pin 9 for the Servo[span_0](end_span)
const int startButtonPin = 2;    [span_1](start_span)// Physical button to start the cycle[span_1](end_span)
const int sprayAngle = 42;       [span_2](start_span)// The "Squirt" angle[span_2](end_span)
const int restAngle = 0;         // The "Home" position

// --- Timing and State ---
RTC_DS3231 rtc;
Servo sprayer;
unsigned long cycleStartUnix = 0; [span_3](start_span)// Saved start time[span_3](end_span)

void setup() {
  Serial.begin(9600);
  
  // Attach the servo using the defined constant
  sprayer.attach(servoPin); 
  sprayer.write(restAngle);
  
  [span_4](start_span)pinMode(startButtonPin, INPUT_PULLUP);[span_4](end_span)

  [span_5](start_span)if (!rtc.begin()) {[span_5](end_span)
    while (1); [span_6](start_span)// Stop if RTC is missing[span_6](end_span)
  }

  // Retrieve the last saved start time from permanent memory
  [span_7](start_span)EEPROM.get(0, cycleStartUnix);[span_7](end_span)
}

void loop() {
  [span_8](start_span)DateTime now = rtc.now();[span_8](end_span)
  [span_9](start_span)uint32_t nowUnix = now.unixtime();[span_9](end_span)

  // 1. Check for Manual Start (Physical Button or Bluetooth 'G')
  [span_10](start_span)if (digitalRead(startButtonPin) == LOW || (Serial.available() && Serial.read() == 'G')) {[span_10](end_span)
    [span_11](start_span)cycleStartUnix = nowUnix;[span_11](end_span)
    EEPROM.put(0, cycleStartUnix); [span_12](start_span)// Save to EEPROM[span_12](end_span)
    Serial.println("Cycle Started Now!");
    delay(500); [span_13](start_span)// Debounce[span_13](end_span)
  }

  // 2. Run the 24-hour logic if a cycle has been set
  [span_14](start_span)if (cycleStartUnix > 0) {[span_14](end_span)
    [span_15](start_span)uint32_t secondsElapsed = nowUnix - cycleStartUnix;[span_15](end_span)
    [span_16](start_span)uint32_t twentyFourHours = 86400;[span_16](end_span)
    
    [span_17](start_span)uint32_t progressInCycle = secondsElapsed % twentyFourHours;[span_17](end_span)
    [span_18](start_span)float hoursIn = progressInCycle / 3600.0;[span_18](end_span)

    // 8-hour Active Window
    [span_19](start_span)if (hoursIn < 8.0) {[span_19](end_span)
      [span_20](start_span)runSchedule(hoursIn);[span_20](end_span)
    }
  }
}

void runSchedule(float h) {
  [span_21](start_span)static int lastSprayedInterval = -1;[span_21](end_span)

  [span_22](start_span)// Ankle Bone Sequence (Hour 1 to 2)[span_22](end_span)
  if (h >= 1.1 && h < 1.15 && lastSprayedInterval != 1) { trigger(1); lastSprayedInterval = 1; [span_23](start_span)}
  if (h >= 1.2 && h < 1.25 && lastSprayedInterval != 2) { trigger(2); lastSprayedInterval = 2; }[span_23](end_span)
  if (h >= 1.5 && h < 1.55 && lastSprayedInterval != 3) { trigger(3); lastSprayedInterval = 3; [span_24](start_span)}
  if (h >= 2.0 && h < 2.05 && lastSprayedInterval != 4) { trigger(4); lastSprayedInterval = 4; }[span_24](end_span)

  [span_25](start_span)// General 2-hour intervals[span_25](end_span)
  if (h >= 4.0 && h < 4.05 && lastSprayedInterval != 5) { trigger(5); lastSprayedInterval = 5; [span_26](start_span)}
  if (h >= 6.0 && h < 6.05 && lastSprayedInterval != 6) { trigger(6); lastSprayedInterval = 6; }[span_26](end_span)
  
  // Reset for the next day
  [span_27](start_span)if (h < 0.1) lastSprayedInterval = -1;[span_27](end_span)
}

void trigger(int count) {
  Serial.print("Spraying interval: ");
  [span_28](start_span)Serial.println(count);[span_28](end_span)
  [span_29](start_span)sprayer.write(sprayAngle);[span_29](end_span)
  [span_30](start_span)delay(1000);[span_30](end_span)
  [span_31](start_span)sprayer.write(restAngle);[span_31](end_span)
}