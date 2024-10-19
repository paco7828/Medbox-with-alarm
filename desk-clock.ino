#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

int alarmSet = true;

void setup() {
  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();

  showCurrentYear(now);
  showCurrentTime(now);
  Serial.println(getCurrentDay(now));
  Serial.println("Alarm: " + getAlarmState());
}

void loop() {
  // Nothing yet
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
  String correctHour = String(now.hour());
  String correctMin = String(now.minute());
  String correctSec = String(now.second());
  correctHour = getCorrectValue(correctHour);
  correctMin = getCorrectValue(correctMin);
  correctSec = getCorrectValue(correctSec);
  Serial.print(correctHour + ":" + correctMin + ":" + correctSec);
  Serial.println();
}

String getCorrectValue(String timePart){
  return timePart.length() < 2 ? "0" + timePart : timePart;
}

String getCurrentDay(DateTime now) {
  switch (now.dayOfTheWeek()) {
    case 0:
      return "Sunday";
    case 1:
      return "Monday";
    case 2:
      return "Tuesday";
    case 3:
      return "Wednesday";
    case 4:
      return "Thursday";
    case 5:
      return "Friday";
    case 6:
      return "Saturday";
  }
  return "";
}

String getAlarmState() {
  return alarmSet ? "ON" : "OFF";
}