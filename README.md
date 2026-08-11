# Timed Box

Battery-powered time-locked box driven by an ESP32-C3 Supermini. It functions as an interactive alarm clock that uses a servo motor to automatically open at a specific time. It utilizes deep sleep mode to preserve battery life when not actively in use.

## Features

- **Time-Triggered Lockbox:** Automatically triggers a servo to open the box when the programmed alarm goes off. The box automatically closes after 30 seconds.
- **Ultra-Low Power Mode:** Enters deep sleep after 60 seconds of inactivity. Automatically wakes up exactly when the alarm is due or via a manual button press.
- **1.8" TFT Display:** Shows real-time clock, date, day of the week, and current alarm status with optimized rendering.
- **Precision Timekeeping:** Uses a DS3231M RTC circuit to maintain accurate time independently of the MCU.
- **Audio Feedback:** Integrated buzzer plays alarm sequences and button interaction sounds.
- **Manual Override:** Long-pressing the main button allows you to manually open or close the box at any time without an alarm.
- **On-Device Configuration:** Two physical buttons to toggle the alarm state, configure the alarm time, and wake the device from sleep. Alarm times are saved in non-volatile memory.
- **Integrated Power Management:** Runs on a 3.7V LiPo battery. Includes a built-in USB-C charging circuit with battery protection and a boost converter for 5V components.

## Hardware Components

- **MCU:** ESP32-C3 SUPERMINI
- **RTC:** DS3231M
- **Display:** 1.8" TFT Screen (ST7735)
- **Actuator:** Servo Motor
- **Audio:** Active Buzzer
- **Power System:** 3.7V LiPo Battery, TP4056 Battery Charger / Protection module (with 8205A & DW01A), MT3608 Step-up Converter (3.7V -> 5V)
- **Inputs:** 2x Tactile Push Buttons, Power Switch