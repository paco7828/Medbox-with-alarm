#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <EEPROM.h>

// Define TFT pins
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8

// Initialize RTC
RTC_DS3231 rtc;

// Initialize TFT screen
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Helper variables
bool alarmSet = true;
String days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
bool onMainScreen;
bool isHourSelected;

// Variables for potmeter values
int potValue;
int lastPotValue;

// Initialize additional pins
int alarmPin = 7;
int backBtn = 6;
int confirmBtn = 5;
int potPin = A0;

// Boolean variables for button states
bool backBtnState;
bool confirmBtnState;

// Get alarm values from EEPROM
int alarmHourInt = EEPROM.read(0);
int alarmMinuteInt = EEPROM.read(1);

void setup() {
  // Set up screen
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(1);

  // Set up RTC
  if (!rtc.begin()) {
    tft.setCursor(30, 55);
    tft.setTextSize(1);
    tft.print("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    tft.setCursor(30, 45);
    tft.setTextSize(1);
    tft.print("RTC lost power,");
    tft.setCursor(30, 55);
    tft.print("setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Load main screen by default
  showMainScreen(rtc.now());

  // Set pins
  pinMode(alarmPin, OUTPUT);
  pinMode(backBtn, INPUT);
  pinMode(confirmBtn, INPUT);
}

void loop() {
  DateTime now = rtc.now();
  static int lastHour = -1;
  static int lastMinute = -1;
  static int lastSecond = -1;
  static String lastDay = "";
  static String lastYMD = "";

  // Get current values
  int currentHour = now.hour();
  int currentMinute = now.minute();
  int currentSecond = now.second();
  String currentDay = getCurrentDay(now);
  String currentYMD = getCurrentYMD(now);

  if (onMainScreen) {
    if (alarmSet) {
      // Start alarm at specified time
      if (currentHour == alarmHourInt && currentMinute == alarmMinuteInt) {
        triggerAlarm();
        // When back button is pressed turn off alarm
        bool backBtnCurrentState = debounceBtn(backBtn, backBtnState);
        if (backBtnCurrentState == HIGH && backBtnState == LOW) {
          alarmSet = false;
          backBtnState = HIGH;
        } else if (backBtnCurrentState == LOW && backBtnState == HIGH) {
          backBtnState = LOW;
        }
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
    if (currentDay != lastDay) {
      lastDay = currentDay;
      updateCurrentDay(currentDay);
    }
    if (currentYMD != lastYMD) {
      lastYMD = currentYMD;
      updateCurrentYMD(currentYMD);
    }

    // When backBtn is pressed
    bool backBtnCurrentState = debounceBtn(backBtn, backBtnState);
    if (backBtnCurrentState == HIGH && backBtnState == LOW) {
      if (!onMainScreen) {
        showMainScreen(now);
      } else {
        // Update the alarm indicator on the screen
        alarmSet = !alarmSet;
        updateAlarmIndicator();
      }
      backBtnState = HIGH;
    } else if (backBtnCurrentState == LOW && backBtnState == HIGH) {
      backBtnState = LOW;
    }

    // When confirmBtn is pressed
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      // Show daily alarm screen
      showDailyAlarm();
      confirmBtnState = HIGH;
    } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
      confirmBtnState = LOW;
    }
  }
}

// Functions to update each values on the main screen
void updateCurrentHour(int hour) {
  tft.setCursor(10, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_WHITE);
  tft.fillRect(10, 35, 50, 40, ST7735_BLACK);
  tft.print(getCorrectValue(String(hour)) + ":");
}

void updateCurrentMinute(int minute) {
  tft.setCursor(60, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_WHITE);
  tft.fillRect(60, 35, 50, 40, ST7735_BLACK);
  tft.print(getCorrectValue(String(minute)) + ":");
}

void updateCurrentSecond(int second) {
  tft.setCursor(115, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_WHITE);
  tft.fillRect(115, 35, 50, 40, ST7735_BLACK);
  tft.print(getCorrectValue(String(second)));
}

void updateCurrentDay(String day) {
  tft.setCursor(40, 75);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.fillRect(40, 75, 100, 20, ST7735_BLACK);
  tft.print(day);
}

void updateCurrentYMD(String ymd) {
  tft.setCursor(20, 110);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.fillRect(20, 110, 120, 20, ST7735_BLACK);
  tft.print(ymd);
}

void updateAlarmIndicator() {
  tft.fillRect(10, 5, 140, 20, alarmSet ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(16, 8);
  tft.setTextSize(2);
  tft.setTextColor(alarmSet ? ST7735_BLACK : ST7735_WHITE);
  tft.print("Alarm " + (alarmHourInt != 255 ? getCorrectValue(String(alarmHourInt)) : "--") + ":" + (alarmMinuteInt != 255 ? getCorrectValue(String(alarmMinuteInt)) : "--"));
}

void showMainScreen(DateTime now) {
  onMainScreen = true;
  // Reset main screen and update values
  tft.fillScreen(ST7735_BLACK);
  updateAlarmIndicator();
  updateCurrentHour(now.hour());
  updateCurrentMinute(now.minute());
  updateCurrentSecond(now.second());
  updateCurrentDay(getCurrentDay(now));
  updateCurrentYMD(getCurrentYMD(now));
}

// Function to return the corresponding value
String getCurrentYMD(DateTime now) {
  return String(now.year()) + "-" + String(now.month()) + "-" + String(now.day());
}

String getCorrectValue(String timePart) {
  return timePart.length() < 2 ? "0" + timePart : timePart;
}

String getCurrentDay(DateTime now) {
  return days[now.dayOfTheWeek()];
}

// Alarm sound function
void triggerAlarm() {
  tone(alarmPin, 2000, 100);
  delay(500);
}

// Function for button debouncing
bool debounceBtn(int btn, bool& lastState) {
  bool currentState = digitalRead(btn);
  if (currentState != lastState) {
    delay(50);
    currentState = digitalRead(btn);
  }
  return currentState;
}

// Function to show the daily alarm part
void showDailyAlarm() {
  onMainScreen = false;
  isHourSelected = true;  // Select hour by default
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(15, 10);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.print("Daily alarm");
  // Initially display "00:00"
  tft.setCursor(35, 50);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_WHITE);
  tft.print("00:00");

  int currentHour = 0;
  int currentMinute = 0;

  // While we are selecting hours
  while (isHourSelected && !onMainScreen) {
    potValue = analogRead(potPin);
    currentHour = map(potValue, 0, 1023, 0, 23);  // Map potmeter to hour range

    // Only update if the hour has changed
    if (currentHour != lastPotValue) {
      lastPotValue = currentHour;
      updateAlarmHour(getCorrectValue(String(currentHour)));
    }

    // Handle button press to confirm hour selection
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      EEPROM.write(0, currentHour);
      isHourSelected = false;
      alarmHourInt = currentHour;
      showMinuteSelection(currentHour);
    } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
      confirmBtnState = LOW;
    }
  }
}

void showMinuteSelection(int selectedHour) {
  int currentMinute = 0;

  while (!isHourSelected) {
    potValue = analogRead(potPin);
    currentMinute = map(potValue, 0, 1023, 0, 59);

    // Only update if the minute has changed
    if (currentMinute != lastPotValue) {
      lastPotValue = currentMinute;
      updateAlarmMinute(getCorrectValue(String(currentMinute)));  // Update minute display
    }

    // Handle button press to confirm minute selection
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      EEPROM.write(1, currentMinute);
      alarmMinuteInt = currentMinute;
      isHourSelected = true;
      onMainScreen = true;
      showMainScreen(rtc.now());
    } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
      confirmBtnState = LOW;
    }
  }
}

void updateAlarmHour(String hour) {
  tft.setCursor(35, 50);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_GREEN);
  tft.fillRect(34, 49, 40, 23, ST7735_BLACK);
  tft.print(hour);
}

void updateAlarmMinute(String minute) {
  tft.setCursor(85, 50);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_GREEN);
  tft.fillRect(81, 49, 41, 23, ST7735_BLACK);
  tft.print(minute);
}
