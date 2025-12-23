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

// Instances
Servo boxServo;
RTC_DS3231 rtc;
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);
Preferences preferences;

// RTC status
bool rtcFound = false;

// Helper variables
constexpr uint8_t BOX_OPEN_FOR = 20;  // seconds
bool alarmSet = true;
bool onMainScreen = true;
bool isHourSelected = true;
bool alarmActive = false;
bool boxOpen = false;
bool alarmStoppedForCurrentTime = false;
unsigned long boxOpenStartTime = 0;

// Button state variables
const unsigned long doubleClickDelay = 400;
const unsigned long debounceDelay = 50;
bool lastBtn1State = HIGH;
bool lastBtn2State = HIGH;
unsigned long lastBtn1Press = 0;
unsigned long lastBtn2Press = 0;
unsigned long btn2FirstClick = 0;

// Alarm values
int alarmHourInt = 0;
int alarmMinuteInt = 0;

// Temporary alarm setting variables
int tempAlarmHour = 0;
int tempAlarmMinute = 0;

void setup() {
  // Initialize Preferences
  preferences.begin("alarm-clock", false);

  // Load saved alarm values
  alarmHourInt = preferences.getInt("alarmHour", 0);
  alarmMinuteInt = preferences.getInt("alarmMin", 0);

  // Set up TFT backlight
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  // Set up screen
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);

  // Set up RTC with custom I2C pins
  Wire.begin(RTC_SDA, RTC_SCL);
  if (!rtc.begin()) {
    rtcFound = false;
  } else {
    rtcFound = true;
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // Set pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  boxServo.attach(SERVO_PIN);
  boxServo.write(0);

  // Check valid alarm values
  if (alarmHourInt > 23) alarmHourInt = 0;
  if (alarmMinuteInt > 59) alarmMinuteInt = 0;

  // Load main screen by default
  if (rtcFound) {
    showMainScreen(rtc.now());
  } else {
    showMainScreenNoRTC();
  }

  // Startup beep
  tone(BUZZER_PIN, 2000, 100);
  delay(150);
  tone(BUZZER_PIN, 2500, 100);
}

void loop() {
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

    if (alarmSet) {
      if (currentHour != alarmHourInt || currentMinute != alarmMinuteInt) {
        alarmStoppedForCurrentTime = false;
      }

      if (currentHour == alarmHourInt && currentMinute == alarmMinuteInt && currentSecond == 0 && !alarmStoppedForCurrentTime) {
        triggerAlarm();
        alarmStoppedForCurrentTime = true;
      }

      if (boxOpen && millis() - boxOpenStartTime >= BOX_OPEN_FOR * 1000) {
        boxServo.write(0);  // Close the box
        boxOpen = false;
        alarmActive = false;
      }
    }

    // Update only changed values
    if (currentSecond != lastSecond) {
      lastSecond = currentSecond;
      updateCurrentSecond(currentSecond);
    }
    if (currentMinute != lastMinute) {
      lastMinute = currentMinute;
      updateCurrentMinute(currentMinute);
    }
    if (currentHour != lastHour) {
      lastHour = currentHour;
      updateCurrentHour(currentHour);
    }
  }

  if (onMainScreen) {
    handleMainScreenButtons();
  } else {
    handleAlarmSettingButtons();
  }
}

void handleMainScreenButtons() {
  // BTN1: Toggle alarm on/off
  if (buttonPressed(BTN1_PIN, lastBtn1State, lastBtn1Press)) {
    alarmSet = !alarmSet;
    updateAlarmIndicator();
    tone(BUZZER_PIN, 1500, 50);
  }

  // BTN2: Enter alarm setting mode
  if (buttonPressed(BTN2_PIN, lastBtn2State, lastBtn2Press)) {
    showDailyAlarm();
    tone(BUZZER_PIN, 2000, 50);
  }
}

void handleAlarmSettingButtons() {
  static bool waitingForDoubleClick = false;
  static unsigned long singleClickTime = 0;

  // BTN1: Increment hour or minute
  if (buttonPressed(BTN1_PIN, lastBtn1State, lastBtn1Press)) {
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
  }

  // BTN2: Decrement or double-click to confirm
  bool btn2Pressed = buttonPressed(BTN2_PIN, lastBtn2State, lastBtn2Press);

  if (btn2Pressed) {
    unsigned long currentTime = millis();

    // Check if double-click
    if (waitingForDoubleClick && (currentTime - singleClickTime < doubleClickDelay)) {
      waitingForDoubleClick = false;
      btn2FirstClick = 0;

      if (isHourSelected) {
        preferences.putInt("alarmHour", tempAlarmHour);
        alarmHourInt = tempAlarmHour;
        isHourSelected = false;

        // Unhighlight hour, highlight minute
        tft.fillRect(34, 49, 40, 23, ST77XX_BLACK);
        tft.setCursor(35, 50);
        tft.setTextSize(3);
        tft.setTextColor(ST77XX_WHITE);
        tft.print(getCorrectValue(String(tempAlarmHour)));

        tone(BUZZER_PIN, 2500, 100);
        delay(100);
        tone(BUZZER_PIN, 3000, 100);
      } else {
        // Confirm minute, save and return to main screen
        preferences.putInt("alarmMin", tempAlarmMinute);
        alarmMinuteInt = tempAlarmMinute;
        onMainScreen = true;
        isHourSelected = true;

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
bool buttonPressed(int pin, bool &lastState, unsigned long &lastPressTime) {
  bool currentState = digitalRead(pin);

  if (currentState == LOW && lastState == HIGH) {
    if (millis() - lastPressTime > debounceDelay) {
      lastPressTime = millis();
      lastState = LOW;
      return true;
    }
  } else if (currentState == HIGH && lastState == LOW) {
    lastState = HIGH;
  }

  return false;
}

void updateCurrentHour(int hour) {
  tft.fillRect(0, 45, 80, 30, ST77XX_BLACK);
  String hourStr = getCorrectValue(String(hour)) + ":";
  drawCenteredText(hourStr, 45, 3, ST77XX_WHITE);
}

void updateCurrentMinute(int minute) {
  tft.fillRect(80, 45, 80, 30, ST77XX_BLACK);
  String minuteStr = getCorrectValue(String(minute)) + ":";
  drawCenteredText(minuteStr, 45, 3, ST77XX_WHITE);
}

void updateCurrentSecond(int second) {
  tft.fillRect(140, 45, 20, 30, ST77XX_BLACK);
  String secondStr = getCorrectValue(String(second));
  drawCenteredText(secondStr, 45, 3, ST77XX_WHITE);
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
  String timeStr = getCorrectValue(String(now.hour())) + ":" + getCorrectValue(String(now.minute())) + ":" + getCorrectValue(String(now.second()));
  drawCenteredText(timeStr, 45, 3, ST77XX_WHITE);
  updateDateDisplay(now);
  updateDayDisplay(now);
}

void showMainScreenNoRTC() {
  onMainScreen = true;
  tft.fillScreen(ST77XX_BLACK);
  updateAlarmIndicator();
  drawCenteredText("00:00:00", 45, 3, ST77XX_WHITE);
  drawCenteredText("YYYY-MM-DD", 80, 2, ST77XX_WHITE);
  drawCenteredText("Unknown", 105, 2, ST77XX_CYAN);
}

String getCorrectValue(String timePart) {
  return timePart.length() < 2 ? "0" + timePart : timePart;
}

// Alarm sound function
void triggerAlarm() {
  static unsigned long lastCycle = 0;
  static int beepStep = 0;
  unsigned long now = millis();

  alarmActive = true;

  if (!boxOpen) {
    boxServo.write(90);
    boxOpen = true;
    boxOpenStartTime = millis();
  }

  if (beepStep < 6) {
    if (now - lastCycle >= 150) {
      lastCycle = now;

      if (beepStep % 2 == 0) {
        tone(BUZZER_PIN, 3700);
      } else {
        noTone(BUZZER_PIN);
      }

      beepStep++;
    }
  } else {
    noTone(BUZZER_PIN);
    if (now - lastCycle >= 2000) {
      beepStep = 0;
      lastCycle = now;
    }
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