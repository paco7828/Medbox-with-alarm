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

int alarmSet = false;
String days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

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

void updateCurrentHour(int hour) {
  tft.setCursor(10, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_WHITE);
  
  // Clear the hour area before updating
  tft.fillRect(10, 35, 50, 40, ST7735_BLACK);
  tft.print(getCorrectValue(String(hour)) + ":");
}

void updateCurrentMinute(int minute) {
  tft.setCursor(60, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_WHITE);
  
  // Clear the minute area before updating
  tft.fillRect(60, 35, 50, 40, ST7735_BLACK);
  tft.print(getCorrectValue(String(minute)) + ":");
}

void updateCurrentSecond(int second) {
  tft.setCursor(115, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_WHITE);
  
  // Clear the second area before updating
  tft.fillRect(115, 35, 50, 40, ST7735_BLACK);
  tft.print(getCorrectValue(String(second)));
}

void updateCurrentDay(String day) {
  tft.setCursor(30, 75);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  
  // Clear the area before updating the day
  tft.fillRect(30, 75, 100, 20, ST7735_BLACK);
  tft.print(day);
}

void updateCurrentYMD(String ymd) {
  tft.setCursor(20, 110);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  
  // Clear the area before updating the YMD
  tft.fillRect(20, 110, 120, 20, ST7735_BLACK);
  tft.print(ymd);
}

void showMainScreen(DateTime now) {
  // Reset screen
  tft.fillScreen(ST7735_BLACK);
  
  // Print current alarm state
  tft.fillRect(45, 5, 70, 20, alarmSet ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(51, 8);
  tft.setTextSize(2);
  tft.setTextColor(alarmSet ? ST7735_BLACK : ST7735_WHITE);
  tft.print("Alarm");

  // Initialize time and day display
  updateCurrentHour(now.hour());
  updateCurrentMinute(now.minute());
  updateCurrentSecond(now.second());
  updateCurrentDay(getCurrentDay(now));
  updateCurrentYMD(getCurrentYMD(now));
}

// Functions to return the corresponding value
String getCurrentYMD(DateTime now) {
  return String(now.year()) + "-" + String(now.month()) + "-" + String(now.day());
}

String getCorrectValue(String timePart) {
  return timePart.length() < 2 ? "0" + timePart : timePart;
}

String getCurrentDay(DateTime now) {
  return days[now.dayOfTheWeek()];
}
