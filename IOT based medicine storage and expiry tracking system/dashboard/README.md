# Web Dashboard

Frontend dashboard for monitoring

## Features
- Live temperature display
- Medicine inventory table
- Expiry alerts
- Add medicine via web
- ESP32 IP-based connection

## How to Run
1. Power on ESP32
2. Note IP from Serial Monitor
3. Open `index.html` in browser
4. Enter ESP32 IP
5. Click Connect

## APIs Used
- GET /api/sensors
- GET /api/inventory
- POST /api/add
- GET /api/scan
