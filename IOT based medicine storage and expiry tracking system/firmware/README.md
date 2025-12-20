# ESP32 Firmware

## Hardware Used
- ESP32
- MFRC522 RFID Reader
- DS18B20 Temperature Sensor
- DS3231 RTC
- SSD1306 OLED
- Buzzer + Switch

## Setup
1. Open esp32_backend.ino in Arduino IDE
2. Create a config.h file using config.example.h
3. Add WiFi credentials
4. Upload to ESP32
5. Check Serial Monitor for IP address

## REST API Endpoints
| Method | Endpoint | Description |
|------|--------|------------|
| GET | /api/sensors | Temperature & alerts |
| GET | /api/inventory | Medicine list |
| POST | /api/add | Add medicine |
| GET | /api/scan | Scan RFID |
| POST | /api/clear | Reset inventory |
