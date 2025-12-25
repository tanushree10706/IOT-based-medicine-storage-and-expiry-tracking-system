# IOT-based-medicine-storage-and-expiry-tracking-system
An IoT-based Smart Medicine Fridge designed to prevent the use of expired medicines, reduce wastage, and ensure proper storage conditions using RFID, temperature monitoring, and a web dashboard.

DEMO VIDEO:https://youtube.com/shorts/c6Mkm95r_cs

🚨 Problem Statement
Households and small clinics often:
Forget medicine expiry dates,
Store medicines at improper temperatures,
Lack timely alerts and inventory tracking.
This leads to health risks and medicine wastage.

💡 Solution
The Medicine Inventory automates medicine management by:
Identifying medicines using RFID,
Monitoring temperature (2–8°C) in real time,
Calculating expiry dates automatically,
Triggering visual, audible, and web alerts and
Providing access via a web dashboard

✨ Key Features
 RFID-based medicine tracking,
 Real-time temperature monitoring,
 Automatic expiry calculation,
 Multi-level alerts (normal alert:2-3 days left ,critical alert:1-0days left),
 Web dashboard for monitoring.

🛠 Tech Stack

Hardware:
ESP32,
MFRC522 RFID Reader,
DS18B20 Temperature Sensor,
DS3231 RTC,
SSD1306 OLED Display,
Buzzer.                
Connections and information about the hardware:https://youtube.com/shorts/zfK9t2jEbow

Software:
Embedded C / Arduino (ESP32),
HTML, CSS, JavaScript (Web Dashboard),
REST API with CORS support and
JSON-based communication.

🧠 System Overview
Scan medicine → RFID UID detected → 
ESP32 matches medicine & expiry → 
System calculates days remaining → 
Alerts triggered if needed → 
Data shown on OLED + Web Dashboard.

📊 Results
 83% reduction in medicine wastage,
 99.8% RFID accuracy,
 ±0.3°C temperature accuracy and
 <500ms RFID response time

🏆 Use Cases
Homes,
Clinics,
Pharmacies,
Elderly care centers.

DOCUMENTATION :https://docs.google.com/document/d/1Z6iB9rLD_7uVRQOJeFIeXZT6NePBG1tPCVhjSqxJvQE/edit?usp=sharing
