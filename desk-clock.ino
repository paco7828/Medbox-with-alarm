#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

int alarmSet = true;
String days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup() {
  Serial.begin(9600);
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  displayCurrentTime();
}

void loop() {
  DateTime now = rtc.now();
  static int lastSecond = -1;
  int currentSecond = now.second();

  if (currentSecond != lastSecond) {
    lastSecond = currentSecond;
    displayCurrentTime();
  }
}

void displayCurrentTime() {
  DateTime now = rtc.now();
  showCurrentYear(now);
  showCurrentTime(now);
  Serial.println(getCurrentDay(now));
  Serial.println("Alarm: " + getAlarmState());
}

void showCurrentYear(DateTime now) {
  Serial.print(now.year(), DEC);
  Serial.print("-");
  Serial.print(now.month(), DEC);
  Serial.print("-");
  Serial.print(now.day(), DEC);
  Serial.println();
}

void showCurrentTime(DateTime now) {
  String correctHour = getCorrectValue(String(now.hour()));
  String correctMin = getCorrectValue(String(now.minute()));
  String correctSec = getCorrectValue(String(now.second()));
  
  Serial.print(correctHour + ":" + correctMin + ":" + correctSec);
  Serial.println();
}

String getCorrectValue(String timePart) {
  return timePart.length() < 2 ? "0" + timePart : timePart;
}

String getCurrentDay(DateTime now) {
  return days[now.dayOfTheWeek()];
}

String getAlarmState() {
  return alarmSet ? "ON" : "OFF";
}
