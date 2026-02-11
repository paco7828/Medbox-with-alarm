#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Preferences.h>
#include <ESP32Servo.h>

// Pins
constexpr uint8_t TFT_CS = 9;
constexpr uint8_t TFT_DC = 5;
constexpr uint8_t TFT_RST = 8;
constexpr uint8_t TFT_MOSI = 4;
constexpr uint8_t TFT_SCK = 2;
constexpr uint8_t TFT_LED = 3;
constexpr uint8_t SERVO_PIN = 0;
constexpr uint8_t BTN1_PIN = 1;
constexpr uint8_t BTN2_PIN = 10;
constexpr uint8_t BUZZER_PIN = 20;
constexpr uint8_t RTC_SDA = 6;
constexpr uint8_t RTC_SCL = 7;

// Deep sleep
constexpr uint32_t AWAKE_TIME_MS = 120000;        // 2 minutes (ms)
constexpr uint32_t MIN_SLEEP_TIME_US = 60000000;  // 1 minute minimum (µs)
constexpr uint32_t ALARM_WAKEUP_MARGIN_MIN = 1;   // Wake up 1 minute before alarm
unsigned long wakeTime = 0;

// Wakeup reason tracking - preserved across deep sleep
RTC_DATA_ATTR int wakeupCount = 0;
RTC_DATA_ATTR bool wasAlarmTriggered = false;
RTC_DATA_ATTR int lastAlarmHour = -1;
RTC_DATA_ATTR int lastAlarmMinute = -1;

// Instances
Servo boxServo;
RTC_DS3231 rtc;
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);
Preferences preferences;

// RTC status
bool rtcFound = false;

// Helper variables
constexpr uint8_t BOX_OPEN_FOR = 30;  // seconds
bool alarmSet = true;
bool onMainScreen = true;
bool isHourSelected = true;
bool alarmActive = false;
bool boxOpen = false;
bool alarmStoppedForCurrentTime = false;
unsigned long boxOpenStartTime = 0;

// Button state variables
bool btn1LastReading = HIGH;
bool btn2LastReading = HIGH;
unsigned long btn1LastDebounceTime = 0;
unsigned long btn2LastDebounceTime = 0;
bool btn1State = HIGH;
bool btn2State = HIGH;
bool btn1LastStableState = HIGH;
bool btn2LastStableState = HIGH;

// Button timing constants
const unsigned long doubleClickDelay = 400;
const unsigned long debounceDelay = 50;
unsigned long btn2FirstClick = 0;

// Alarm sound control
unsigned long lastBeepCycle = 0;
int beepState = 0;
bool alarmSoundActive = false;

// Alarm values
int alarmHourInt = 0;
int alarmMinuteInt = 0;

// Temporary alarm setting variables
int tempAlarmHour = 0;
int tempAlarmMinute = 0;

// Time display positioning and optimization
const int TIME_Y = 45;
const int TIME_SIZE = 3;

// Previous time values for optimization
int prevHourTens = -1;
int prevHourOnes = -1;
int prevMinTens = -1;
int prevMinOnes = -1;
int prevSecTens = -1;
int prevSecOnes = -1;

void setup() {
  wakeTime = millis();

  // Check wakeup reason FIRST
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool isGpioWakeup = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO);
  bool isFirstBoot = (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED);
  bool isTimerWakeup = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);

  wakeupCount++;

  // Initialize Preferences
  preferences.begin("alarm-clock", false);

  // Load saved alarm values
  alarmHourInt = preferences.getInt("alarmHour", 0);
  alarmMinuteInt = preferences.getInt("alarmMin", 0);
  alarmSet = preferences.getBool("alarmSet", true);

  // Set up TFT backlight
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  // Set up screen
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(3);

  // Set up RTC with custom I2C pins
  Wire.begin(RTC_SDA, RTC_SCL);
  if (!rtc.begin()) {
    rtcFound = false;
  } else {
    rtcFound = true;

    // Check if time was already set manually
    bool timeWasSet = preferences.getBool("timeSet", false);

    if (!timeWasSet) {
      // MANUALLY SET THE TIME HERE ONLY ONCE!
      // Format: DateTime(year, month, day, hour, minute, second)
      rtc.adjust(DateTime(2025, 1, 30, 15, 33, 27));

      // Save flag so it doesn't reset again
      preferences.putBool("timeSet", true);
    }
  }

  // Set pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  boxServo.attach(SERVO_PIN);

  // Check valid alarm values
  if (alarmHourInt > 23) alarmHourInt = 0;
  if (alarmMinuteInt > 59) alarmMinuteInt = 0;

  // Check if we should trigger alarm
  bool shouldTriggerAlarm = false;
  if (rtcFound && alarmSet) {
    DateTime now = rtc.now();

    // Only trigger if:
    // 1. We're at exact alarm time (hour and minute match)
    // 2. This alarm hasn't been triggered yet (check against last triggered hour/minute)
    if (now.hour() == alarmHourInt && now.minute() == alarmMinuteInt) {
      // Check if this is a new alarm (not already triggered)
      if (lastAlarmHour != alarmHourInt || lastAlarmMinute != alarmMinuteInt) {
        shouldTriggerAlarm = true;
        lastAlarmHour = alarmHourInt;
        lastAlarmMinute = alarmMinuteInt;
      }
    }

    // OR if wasAlarmTriggered flag was set (woke up just before alarm)
    if (wasAlarmTriggered && now.hour() == alarmHourInt && now.minute() == alarmMinuteInt) {
      shouldTriggerAlarm = true;
      wasAlarmTriggered = false;
      lastAlarmHour = alarmHourInt;
      lastAlarmMinute = alarmMinuteInt;
    }

    showMainScreen(now);
  } else {
    showMainScreenNoRTC();
  }

  // Handle wakeup scenarios
  if (shouldTriggerAlarm) {
    // Alarm time - trigger alarm
    triggerAlarm();
    tone(BUZZER_PIN, 2000, 100);
    delay(150);
    tone(BUZZER_PIN, 2500, 100);
  } else if (isGpioWakeup) {
    // GPIO wakeup (button press) - just beep
    tone(BUZZER_PIN, 2000, 100);
    delay(150);
    tone(BUZZER_PIN, 2500, 100);
    boxServo.write(0);  // Ensure closed position
  } else if (isTimerWakeup) {
    // Timer wakeup means we're near alarm time - quick beep only
    tone(BUZZER_PIN, 1500, 50);
    boxServo.write(0);  // Ensure closed position
  } else {
    // First boot - full startup sequence with servo test
    tone(BUZZER_PIN, 2000, 100);
    delay(150);
    tone(BUZZER_PIN, 2500, 100);

    // Servo test cycle (only on first boot)
    boxServo.write(0);
    for (int i = 0; i < 5; i++) {
      boxServo.write(0);
      delay(1000);
      boxServo.write(180);
      delay(1000);
    }
    boxServo.write(0);
  }
}

void loop() {
  if (millis() - wakeTime >= AWAKE_TIME_MS) {
    goToDeepSleep();
  }

  // Handle RTC time updates
  if (rtcFound && onMainScreen) {
    DateTime now = rtc.now();
    static int lastHour = -1;
    static int lastMinute = -1;
    static int lastSecond = -1;
    static String lastDateStr = "";

    String currentDateStr = String(now.year()) + "-" + getCorrectValue(String(now.month())) + "-" + getCorrectValue(String(now.day()));

    // Update date if changed
    if (currentDateStr != lastDateStr) {
      lastDateStr = currentDateStr;
      updateDateDisplay(now);
      updateDayDisplay(now);
    }

    // Get current values
    int currentHour = now.hour();
    int currentMinute = now.minute();
    int currentSecond = now.second();

    // OPTIMIZED: Only update changed digits
    if (currentHour != lastHour || currentMinute != lastMinute || currentSecond != lastSecond) {
      updateTimeDisplayOptimized(currentHour, currentMinute, currentSecond);
      lastHour = currentHour;
      lastMinute = currentMinute;
      lastSecond = currentSecond;
    }

    if (alarmSet) {
      // Reset the alarm if time has passed the alarm time
      if (currentHour != alarmHourInt || currentMinute != alarmMinuteInt) {
        alarmStoppedForCurrentTime = false;
      }

      // Reset last alarm tracking at midnight for next day's alarm
      if (currentHour == 0 && currentMinute == 0 && currentSecond == 0) {
        lastAlarmHour = -1;
        lastAlarmMinute = -1;
      }

      // Check if it's alarm time
      if (currentHour == alarmHourInt && currentMinute == alarmMinuteInt && currentSecond == 0 && !alarmStoppedForCurrentTime) {
        triggerAlarm();
        alarmStoppedForCurrentTime = true;
        lastAlarmHour = alarmHourInt;
        lastAlarmMinute = alarmMinuteInt;
      }

      // Stop alarm after 30 seconds if box closed
      if (boxOpen && millis() - boxOpenStartTime >= BOX_OPEN_FOR * 1000) {
        boxServo.write(0);  // Close the box
        boxOpen = false;
        alarmActive = false;
        alarmSoundActive = false;
        noTone(BUZZER_PIN);
      }
    }
  }

  // If alarm active, continue beeping
  if (alarmActive && alarmSoundActive) {
    handleAlarmSound();
  }

  if (onMainScreen) {
    handleMainScreenButtons();
  } else {
    handleAlarmSettingButtons();
  }
}

void goToDeepSleep() {
  digitalWrite(TFT_LED, LOW);
  tft.enableSleep(true);
  boxServo.detach();

  // GPIO wakeup on button press — always enabled
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN1_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);

  // Timer wakeup only when an alarm is scheduled
  // Sleeps until (alarm time - ALARM_WAKEUP_MARGIN_MIN), no periodic wakeup otherwise
  if (alarmSet && rtcFound) {
    DateTime now = rtc.now();

    int currentTotalMinutes = now.hour() * 60 + now.minute();
    int alarmTotalMinutes = alarmHourInt * 60 + alarmMinuteInt;

    int minutesUntilAlarm = alarmTotalMinutes - currentTotalMinutes;
    if (minutesUntilAlarm <= 0) {
      minutesUntilAlarm += 24 * 60;  // Alarm is tomorrow
    }

    // Subtract margin so we wake up slightly before the alarm
    int wakeupMinutes = minutesUntilAlarm - ALARM_WAKEUP_MARGIN_MIN;
    if (wakeupMinutes < 1) wakeupMinutes = 1;  // Never sleep less than 1 minute

    uint64_t sleepTime = (uint64_t)wakeupMinutes * 60 * 1000000ULL;

    // Enforce absolute minimum sleep time
    if (sleepTime < MIN_SLEEP_TIME_US) {
      sleepTime = MIN_SLEEP_TIME_US;
    }

    wasAlarmTriggered = true;
    esp_sleep_enable_timer_wakeup(sleepTime);
  }
  // No alarm set → no timer registered, device sleeps until button press only

  esp_deep_sleep_start();
}

void handleMainScreenButtons() {
  // If alarm active, button press stops it
  if (alarmActive) {
    if (buttonPressed(BTN1_PIN, btn1LastReading, btn1LastDebounceTime, btn1State, btn1LastStableState) || buttonPressed(BTN2_PIN, btn2LastReading, btn2LastDebounceTime, btn2State, btn2LastStableState)) {

      boxServo.write(0);
      boxOpen = false;
      alarmActive = false;
      alarmSoundActive = false;
      alarmStoppedForCurrentTime = true;
      wasAlarmTriggered = false;  // Clear the flag
      // Keep lastAlarmHour and lastAlarmMinute so it doesn't trigger again today
      noTone(BUZZER_PIN);
      beepState = 0;
      tone(BUZZER_PIN, 1000, 200);  // Confirmation beep
      wakeTime = millis();
      return;
    }
  }

  // Normal operation
  // BTN1: Toggle alarm on/off
  if (buttonPressed(BTN1_PIN, btn1LastReading, btn1LastDebounceTime, btn1State, btn1LastStableState)) {
    alarmSet = !alarmSet;
    preferences.putBool("alarmSet", alarmSet);  // Save alarm state
    updateAlarmIndicator();
    tone(BUZZER_PIN, 1500, 50);
    wakeTime = millis();
  }

  // BTN2: Enter alarm setting mode
  if (buttonPressed(BTN2_PIN, btn2LastReading, btn2LastDebounceTime, btn2State, btn2LastStableState)) {
    showDailyAlarm();
    tone(BUZZER_PIN, 2000, 50);
    wakeTime = millis();
  }
}

void handleAlarmSettingButtons() {
  static bool waitingForDoubleClick = false;
  static unsigned long singleClickTime = 0;

  // BTN1: Increment hour or minute
  if (buttonPressed(BTN1_PIN, btn1LastReading, btn1LastDebounceTime, btn1State, btn1LastStableState)) {
    btn2FirstClick = 0;
    waitingForDoubleClick = false;

    if (isHourSelected) {
      tempAlarmHour = (tempAlarmHour + 1) % 24;
      updateAlarmHour(getCorrectValue(String(tempAlarmHour)));
    } else {
      tempAlarmMinute = (tempAlarmMinute + 1) % 60;
      updateAlarmMinute(getCorrectValue(String(tempAlarmMinute)));
    }
    tone(BUZZER_PIN, 1800, 30);
    wakeTime = millis();
  }

  // BTN2: Decrement or double-click to confirm
  if (buttonPressed(BTN2_PIN, btn2LastReading, btn2LastDebounceTime, btn2State, btn2LastStableState)) {
    unsigned long currentTime = millis();

    if (waitingForDoubleClick && (currentTime - singleClickTime < doubleClickDelay)) {
      // Double-click detected
      waitingForDoubleClick = false;

      if (isHourSelected) {
        // Confirm hour, move to minute
        preferences.putInt("alarmHour", tempAlarmHour);
        alarmHourInt = tempAlarmHour;
        isHourSelected = false;

        tft.setCursor(35, 50);
        tft.setTextSize(3);
        tft.setTextColor(ST77XX_WHITE);
        tft.fillRect(34, 49, 40, 23, ST77XX_BLACK);
        tft.print(getCorrectValue(String(tempAlarmHour)));

        tft.setCursor(85, 50);
        tft.setTextSize(3);
        tft.setTextColor(ST77XX_GREEN);
        tft.fillRect(81, 49, 41, 23, ST77XX_BLACK);
        tft.print(getCorrectValue(String(tempAlarmMinute)));

        tone(BUZZER_PIN, 2500, 100);
        delay(100);
        tone(BUZZER_PIN, 3000, 100);
      } else {
        // Confirm minute, save and return to main screen
        preferences.putInt("alarmMin", tempAlarmMinute);
        alarmMinuteInt = tempAlarmMinute;
        onMainScreen = true;
        isHourSelected = true;

        // Reset last alarm tracking since we set a new alarm
        lastAlarmHour = -1;
        lastAlarmMinute = -1;

        if (rtcFound) {
          showMainScreen(rtc.now());
        } else {
          showMainScreenNoRTC();
        }

        tone(BUZZER_PIN, 2500, 100);
        delay(100);
        tone(BUZZER_PIN, 3000, 100);
        delay(100);
        tone(BUZZER_PIN, 3500, 100);
      }
    } else {
      // Wait for double-click
      waitingForDoubleClick = true;
      singleClickTime = currentTime;
    }
    wakeTime = millis();
  }

  // Check if single-click timeout expired
  if (waitingForDoubleClick && (millis() - singleClickTime >= doubleClickDelay)) {
    waitingForDoubleClick = false;
    if (isHourSelected) {
      tempAlarmHour = (tempAlarmHour - 1 + 24) % 24;
      updateAlarmHour(getCorrectValue(String(tempAlarmHour)));
    } else {
      tempAlarmMinute = (tempAlarmMinute - 1 + 60) % 60;
      updateAlarmMinute(getCorrectValue(String(tempAlarmMinute)));
    }
    tone(BUZZER_PIN, 1600, 30);
  }
}

// Function to handle button press with debouncing
bool buttonPressed(int pin, bool &lastReading, unsigned long &lastDebounceTime, bool &buttonState, bool &lastStableState) {
  bool reading = digitalRead(pin);
  bool pressed = false;

  // If the reading changed reset debounce timer
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  // If enough time has passed, accept the reading as stable
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the stable state changed
    if (reading != buttonState) {
      buttonState = reading;

      // Only register a press on the falling edge (HIGH to LOW)
      if (buttonState == LOW && lastStableState == HIGH) {
        pressed = true;
      }
      lastStableState = buttonState;
    }
  }

  lastReading = reading;
  return pressed;
}

// OPTIMIZED: Only redraw changed digits
void updateTimeDisplayOptimized(int hour, int minute, int second) {
  int hourTens = hour / 10;
  int hourOnes = hour % 10;
  int minTens = minute / 10;
  int minOnes = minute % 10;
  int secTens = second / 10;
  int secOnes = second % 10;

  tft.setTextSize(TIME_SIZE);
  tft.setTextColor(ST77XX_WHITE);

  // Get actual character dimensions
  int16_t x1, y1;
  uint16_t charW, charH;
  tft.getTextBounds("0", 0, 0, &x1, &y1, &charW, &charH);

  uint16_t colonW;
  tft.getTextBounds(":", 0, 0, &x1, &y1, &colonW, &charH);

  // Calculate total width and starting position
  String fullTime = getCorrectValue(String(hour)) + ":" + getCorrectValue(String(minute)) + ":" + getCorrectValue(String(second));
  uint16_t totalW, totalH;
  tft.getTextBounds(fullTime, 0, 0, &x1, &y1, &totalW, &totalH);
  int baseX = (tft.width() - totalW) / 2;

  // Calculate positions for each element
  int pos = baseX;

  // Hour tens
  if (hourTens != prevHourTens) {
    tft.fillRect(pos - 1, TIME_Y - 1, charW + 2, charH + 2, ST77XX_BLACK);
    tft.setCursor(pos, TIME_Y);
    tft.print(hourTens);
    prevHourTens = hourTens;
  }
  pos += charW;

  // Hour ones
  if (hourOnes != prevHourOnes) {
    tft.fillRect(pos - 1, TIME_Y - 1, charW + 2, charH + 2, ST77XX_BLACK);
    tft.setCursor(pos, TIME_Y);
    tft.print(hourOnes);
    prevHourOnes = hourOnes;
  }
  pos += charW;

  // Colon 1
  tft.setCursor(pos, TIME_Y);
  tft.print(":");
  pos += colonW;

  // Minute tens
  if (minTens != prevMinTens) {
    tft.fillRect(pos - 1, TIME_Y - 1, charW + 2, charH + 2, ST77XX_BLACK);
    tft.setCursor(pos, TIME_Y);
    tft.print(minTens);
    prevMinTens = minTens;
  }
  pos += charW;

  // Minute ones
  if (minOnes != prevMinOnes) {
    tft.fillRect(pos - 1, TIME_Y - 1, charW + 2, charH + 2, ST77XX_BLACK);
    tft.setCursor(pos, TIME_Y);
    tft.print(minOnes);
    prevMinOnes = minOnes;
  }
  pos += charW;

  // Colon 2
  tft.setCursor(pos, TIME_Y);
  tft.print(":");
  pos += colonW;

  // Second tens
  if (secTens != prevSecTens) {
    tft.fillRect(pos - 1, TIME_Y - 1, charW + 2, charH + 2, ST77XX_BLACK);
    tft.setCursor(pos, TIME_Y);
    tft.print(secTens);
    prevSecTens = secTens;
  }
  pos += charW;

  // Second ones
  if (secOnes != prevSecOnes) {
    tft.fillRect(pos - 1, TIME_Y - 1, charW + 2, charH + 2, ST77XX_BLACK);
    tft.setCursor(pos, TIME_Y);
    tft.print(secOnes);
    prevSecOnes = secOnes;
  }
}

void updateDateDisplay(DateTime now) {
  String dateStr = String(now.year()) + "-" + getCorrectValue(String(now.month())) + "-" + getCorrectValue(String(now.day()));
  tft.fillRect(0, 80, tft.width(), 20, ST77XX_BLACK);
  drawCenteredText(dateStr, 80, 2, ST77XX_WHITE);
}

void updateDayDisplay(DateTime now) {
  String dayStr = getDayName(now.dayOfTheWeek());
  tft.fillRect(0, 105, tft.width(), 20, ST77XX_BLACK);
  drawCenteredText(dayStr, 105, 2, ST77XX_CYAN);
}

void updateAlarmIndicator() {
  tft.fillRect(0, 5, tft.width(), 25, ST77XX_BLACK);
  tft.fillRect(0, 5, tft.width(), 25, alarmSet ? ST77XX_GREEN : ST77XX_RED);
  String alarmText = "Alarm " + getCorrectValue(String(alarmHourInt)) + ":" + getCorrectValue(String(alarmMinuteInt));
  drawCenteredText(alarmText, 8, 2, alarmSet ? ST77XX_BLACK : ST77XX_WHITE);
}

void showMainScreen(DateTime now) {
  onMainScreen = true;
  tft.fillScreen(ST77XX_BLACK);
  updateAlarmIndicator();

  // Reset previous values to force full redraw
  prevHourTens = -1;
  prevHourOnes = -1;
  prevMinTens = -1;
  prevMinOnes = -1;
  prevSecTens = -1;
  prevSecOnes = -1;

  // Clear the entire time display area to prevent overlapping
  tft.fillRect(0, TIME_Y - 2, tft.width(), 30, ST77XX_BLACK);

  updateTimeDisplayOptimized(now.hour(), now.minute(), now.second());
  updateDateDisplay(now);
  updateDayDisplay(now);
}

void showMainScreenNoRTC() {
  onMainScreen = true;
  tft.fillScreen(ST77XX_BLACK);
  updateAlarmIndicator();

  // Reset previous values
  prevHourTens = -1;
  prevHourOnes = -1;
  prevMinTens = -1;
  prevMinOnes = -1;
  prevSecTens = -1;
  prevSecOnes = -1;

  // Clear the entire time display area to prevent overlapping
  tft.fillRect(0, TIME_Y - 2, tft.width(), 30, ST77XX_BLACK);

  updateTimeDisplayOptimized(0, 0, 0);
  drawCenteredText("YYYY-MM-DD", 80, 2, ST77XX_WHITE);
  drawCenteredText("Unknown", 105, 2, ST77XX_CYAN);
}

String getCorrectValue(String timePart) {
  return timePart.length() < 2 ? "0" + timePart : timePart;
}

void handleAlarmSound() {
  unsigned long now = millis();

  switch (beepState) {
    case 0:  // First beep
      if (now - lastBeepCycle >= 200) {
        tone(BUZZER_PIN, 3700);
        lastBeepCycle = now;
        beepState = 1;
      }
      break;

    case 1:  // Pause after first beep
      if (now - lastBeepCycle >= 200) {
        noTone(BUZZER_PIN);
        lastBeepCycle = now;
        beepState = 2;
      }
      break;

    case 2:  // Second beep
      if (now - lastBeepCycle >= 200) {
        tone(BUZZER_PIN, 3700);
        lastBeepCycle = now;
        beepState = 3;
      }
      break;

    case 3:  // Pause after second beep
      if (now - lastBeepCycle >= 200) {
        noTone(BUZZER_PIN);
        lastBeepCycle = now;
        beepState = 4;
      }
      break;

    case 4:  // Third beep
      if (now - lastBeepCycle >= 200) {
        tone(BUZZER_PIN, 3700);
        lastBeepCycle = now;
        beepState = 5;
      }
      break;

    case 5:  // Pause after third beep
      if (now - lastBeepCycle >= 200) {
        noTone(BUZZER_PIN);
        lastBeepCycle = now;
        beepState = 6;
      }
      break;

    case 6:  // 5 second silence
      noTone(BUZZER_PIN);
      if (now - lastBeepCycle >= 5000) {
        lastBeepCycle = now;
        beepState = 0;  // Restart cycle
      }
      break;
  }
}

void triggerAlarm() {
  alarmActive = true;
  alarmSoundActive = true;
  beepState = 0;
  lastBeepCycle = millis();

  if (!boxOpen) {
    boxServo.write(180);
    boxOpen = true;
    boxOpenStartTime = millis();
  }
}

// Function to show the daily alarm setting screen
void showDailyAlarm() {
  onMainScreen = false;
  isHourSelected = true;
  btn2FirstClick = 0;

  // Init temp values with current alarm
  tempAlarmHour = alarmHourInt;
  tempAlarmMinute = alarmMinuteInt;

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 10);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Set Alarm");

  tft.setCursor(35, 50);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(getCorrectValue(String(tempAlarmHour)));
  tft.setTextColor(ST77XX_WHITE);
  tft.print(":");
  tft.print(getCorrectValue(String(tempAlarmMinute)));

  tft.setCursor(10, 90);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("BTN1: +1");
  tft.setCursor(10, 100);
  tft.print("BTN2: -1");
  tft.setCursor(10, 110);
  tft.print("BTN2 x2: Next/Save");
}

void updateAlarmHour(String hour) {
  tft.setCursor(35, 50);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_GREEN);
  tft.fillRect(34, 49, 40, 23, ST77XX_BLACK);
  tft.print(hour);
}

void updateAlarmMinute(String minute) {
  tft.setCursor(85, 50);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_GREEN);
  tft.fillRect(81, 49, 41, 23, ST77XX_BLACK);
  tft.print(minute);
}

String getDayName(uint8_t dayOfWeek) {
  switch (dayOfWeek) {
    case 0: return "Sunday";
    case 1: return "Monday";
    case 2: return "Tuesday";
    case 3: return "Wednesday";
    case 4: return "Thursday";
    case 5: return "Friday";
    case 6: return "Saturday";
    default: return "Unknown";
  }
}

// Function to center text
void drawCenteredText(String text, int y, int textSize, uint16_t color) {
  tft.setTextSize(textSize);
  tft.setTextColor(color);

  // Calculate width of text
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  // Calculate centered position
  int x = (tft.width() - w) / 2;

  tft.setCursor(x, y);
  tft.print(text);
}
