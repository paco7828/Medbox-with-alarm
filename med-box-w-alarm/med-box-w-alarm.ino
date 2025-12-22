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

// Functions to update each value on the main screen
void updateCurrentHour(int hour) {
  tft.setCursor(10, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.fillRect(10, 35, 50, 40, ST77XX_BLACK);
  tft.print(getCorrectValue(String(hour)) + ":");
}

void updateCurrentMinute(int minute) {
  tft.setCursor(60, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.fillRect(60, 35, 50, 40, ST77XX_BLACK);
  tft.print(getCorrectValue(String(minute)) + ":");
}

void updateCurrentSecond(int second) {
  tft.setCursor(115, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.fillRect(115, 35, 50, 40, ST77XX_BLACK);
  tft.print(getCorrectValue(String(second)));
}

void updateAlarmIndicator() {
  tft.fillRect(10, 5, 140, 20, alarmSet ? ST77XX_GREEN : ST77XX_RED);
  tft.setCursor(16, 8);
  tft.setTextSize(2);
  tft.setTextColor(alarmSet ? ST77XX_BLACK : ST77XX_WHITE);
  String alarmText = "Alarm " + getCorrectValue(String(alarmHourInt)) + ":" + getCorrectValue(String(alarmMinuteInt));
  tft.print(alarmText);
}

void showMainScreen(DateTime now) {
  onMainScreen = true;
  // Reset main screen and update values
  tft.fillScreen(ST77XX_BLACK);
  updateAlarmIndicator();
  updateCurrentHour(now.hour());
  updateCurrentMinute(now.minute());
  updateCurrentSecond(now.second());
}

void showMainScreenNoRTC() {
  onMainScreen = true;
  tft.fillScreen(ST77XX_BLACK);
  updateAlarmIndicator();

  tft.setCursor(10, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("00:00:00");
}

String getCorrectValue(String timePart) {
  return timePart.length() < 2 ? "0" + timePart : timePart;
}

// Alarm sound function
void triggerAlarm() {
  unsigned long prevTime = 0;
  unsigned long currentTime = millis();

  // Activate alarm
  alarmActive = true;

  if (!boxOpen) {
    // Open the box
    boxServo.write(90);
    boxOpen = true;
    boxOpenStartTime = millis();
  }

  // Alarm sound
  if (currentTime - prevTime >= 700) {
    for (int i = 0; i < 5; i++) {
      tone(BUZZER_PIN, 2000);
      delay(100);
      noTone(BUZZER_PIN);
      delay(100);
    }
    prevTime = currentTime;
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