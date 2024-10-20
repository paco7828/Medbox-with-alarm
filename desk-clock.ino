#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Define TFT pins
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8

// Initialize RTC
RTC_DS3231 rtc;

// Initialize TFT screen
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Helper variables
int alarmSet = true;
String days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
bool startAlarm = true;
bool onMainScreen;
int numberOfHours = 24;
int numberOfMinutes = 60; // Added variable for minutes
bool isHourSelected; // Changed to bool for clearer meaning
String lastDisplayedAlarmHour = "";
String lastDisplayedAlarmMinute = ""; // Added to store the last displayed minute

// Initialize additional pins
int alarm = 7;
int backBtn = 6;
int confirmBtn = 5;
int potPin = A0;

// Boolean variables for button states
bool backBtnState;
bool confirmBtnState;

void setup() {
  // Set up screen
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(1);

  // Set up RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
  showMainScreen(now);

  // Set pins
  pinMode(alarm, OUTPUT);
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

  if (alarmSet) {
    // Start alarm at specified time
  }

  if (onMainScreen) {
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
  }

  // When backBtn is pressed
  bool backBtnCurrentState = debounceBtn(backBtn, backBtnState);
  if (backBtnCurrentState == HIGH && backBtnState == LOW) {
    if (!onMainScreen) {
      showMainScreen(now);
    } else {
      // Toggle the alarmSet variable
      alarmSet = !alarmSet;

      // Update the alarm indicator on the screen
      updateAlarmIndicator();
    }
    backBtnState = HIGH;
  } else if (backBtnCurrentState == LOW && backBtnState == HIGH) {
    backBtnState = LOW;
  }

  // When confirmBtn is pressed
  bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
  if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
    if (isHourSelected) {
      showDailyAlarmMinutes(); // Call the function for minute selection if hours are selected
    } else {
      showDailyAlarm(); // Call the function for hour selection
    }
    confirmBtnState = HIGH;
  } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
    confirmBtnState = LOW;
  }
}

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
  // Update the alarm indicator on the screen
  tft.fillRect(45, 5, 70, 20, alarmSet ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(51, 8);
  tft.setTextSize(2);
  tft.setTextColor(alarmSet ? ST7735_BLACK : ST7735_WHITE);
  tft.print("Alarm");
}

void showMainScreen(DateTime now) {
  onMainScreen = true;
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
void alarmTest() {
  tone(alarm, 2000, 100);
  delay(500);
  tone(alarm, 2000, 100);
  delay(500);
  tone(alarm, 2000, 100);
  delay(500);
}

bool debounceBtn(int btn, bool& lastState) {
  bool currentState = digitalRead(btn);
  if (currentState != lastState) {
    delay(50);
    currentState = digitalRead(btn);
  }
  return currentState;
}

void showDailyAlarm() {
  onMainScreen = false;
  isHourSelected = true;
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(15, 10);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.print("Daily alarm");
  tft.setCursor(35, 50);
  tft.setTextSize(3);
  tft.print("00:00");

  int lastPotValue = -1;  // Variable to track the last potentiometer value

  while (isHourSelected) {
    int potValue = analogRead(potPin);
    int currentHour = map(potValue, 0, 1023, 0, numberOfHours - 1);

    // Only update if the hour has changed
    if (currentHour != lastPotValue) {
      lastPotValue = currentHour;
      updateAlarmHour(getCorrectValue(String(currentHour)));
    }
  }
}

void updateAlarmHour(String currentOption) {
  // Clear the area where the hour is displayed
  tft.fillRect(35, 49, 50, 23, ST7735_BLACK);
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(35, 50);
  tft.print(currentOption);
}

void showDailyAlarmMinutes() {
  onMainScreen = false;
  isHourSelected = false; // Indicate we're selecting minutes
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(15, 10);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.print("Select Minute");
  tft.setCursor(35, 50);
  tft.setTextSize(3);
  tft.print("00");

  int lastPotValue = -1;  // Variable to track the last potentiometer value

  while (!isHourSelected) {
    int potValue = analogRead(potPin);
    int currentMinute = map(potValue, 0, 1023, 0, numberOfMinutes - 1);

    // Only update if the minute has changed
    if (currentMinute != lastPotValue) {
      lastPotValue = currentMinute;
      updateAlarmMinute(getCorrectValue(String(currentMinute)));
    }
  }
}

void updateAlarmMinute(String currentOption) {
  // Clear the area where the minute is displayed
  tft.fillRect(35, 49, 40, 23, ST7735_BLACK);
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(35, 50);
  tft.print(currentOption);
}
