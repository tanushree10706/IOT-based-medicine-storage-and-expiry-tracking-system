#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RTClib.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RFID
#define RST_PIN 4
#define SS_PIN 5
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Temperature Sensor
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// RTC
RTC_DS3231 rtc;

// Buzzer
#define BUZZER_PIN 15
#define BUZZER_SWITCH 14

// ================== WIFI SETTINGS ==================
const char* ssid = "123";
const char* password = "tilak123";

// Web Server
WebServer server(80);

// ================== MEDICINE DATA ==================
struct Medicine {
  String name;
  String expiryDate;
  int daysUntilExpiry;
};

// Static database
String knownRFIDs[][3] = {
  {"5A F6 57 80", "DOLO 650", "2025-12-18"},
  {"5A A0 B5 80", "ALM-PLUS", "2025-12-19"},
  {"F3 C9 C9 D9", "UL-SORE GEL", "2026-12-31"},
  {"6A 44 5A 81", "Nutrolin-B", "2025-12-31"}
};

const int MAX_MEDICINES = 20;
int numMedicines = 4;

Medicine currentMedicines[10];
int medicineCount = 0;
bool alertActive = false;
bool buzzerEnabled = true;
unsigned long lastAlertTime = 0;
unsigned long alertStartTime = 0;
const unsigned long MAX_ALERT_TIME = 300000;

// Alert cycling variables
Medicine expiringMedicines[10];
int expiringCount = 0;
int currentAlertIndex = 0;
unsigned long lastAlertChange = 0;
const unsigned long ALERT_DISPLAY_TIME = 15000;

// Non-blocking buzzer variables
unsigned long lastBuzzerTime = 0;
int buzzerState = 0;
int beepCount = 0;
bool isBuzzerActive = false;
bool isNewAlert = true;

// ================== TIME CALCULATION FUNCTIONS ==================
String normalizeUID(String uid) {
  uid.trim();
  uid.toUpperCase();
  String normalized = "";
  for (int i = 0; i < uid.length(); i++) {
    if (uid[i] != ' ' || (i > 0 && uid[i-1] != ' ')) {
      normalized += uid[i];
    }
  }
  normalized.trim();
  return normalized;
}

DateTime getCorrectedTime() {
  DateTime now = rtc.now();
  
  // Add 30 minutes correction
  int correctedMinute = now.minute() + 30;
  int correctedHour = now.hour();
  int correctedDay = now.day();
  int correctedMonth = now.month();
  int correctedYear = now.year();
  
  if (correctedMinute >= 60) {
    correctedMinute -= 60;
    correctedHour += 1;
  }
  if (correctedHour >= 24) {
    correctedHour -= 24;
    correctedDay += 1;
  }
  
  return DateTime(correctedYear, correctedMonth, correctedDay, correctedHour, correctedMinute, now.second());
}

// CORRECTED: Calculate days between two dates
int calculateDaysBetween(DateTime start, DateTime end) {
  // Convert to Unix timestamp and calculate difference in days
  long startSeconds = start.unixtime();
  long endSeconds = end.unixtime();
  
  // Calculate difference in seconds
  long diffSeconds = endSeconds - startSeconds;
  
  // Convert to days (86400 seconds in a day)
  int days = diffSeconds / 86400;
  
  return days;
}

// CORRECTED: Get days until expiry
int getDaysUntilExpiry(String expiryDateStr) {
  DateTime now = getCorrectedTime();
  
  // Parse expiry date
  int expiryYear = expiryDateStr.substring(0,4).toInt();
  int expiryMonth = expiryDateStr.substring(5,7).toInt();
  int expiryDay = expiryDateStr.substring(8,10).toInt();
  
  DateTime expiryDate = DateTime(expiryYear, expiryMonth, expiryDay, 23, 59, 59);
  
  // Calculate days difference
  return calculateDaysBetween(now, expiryDate);
}

void startBuzzerAlert(bool critical) {
  if (!buzzerEnabled || !isNewAlert) return;
  
  isBuzzerActive = true;
  buzzerState = 0;
  beepCount = 0;
  lastBuzzerTime = millis();
  digitalWrite(BUZZER_PIN, LOW);
}

void stopBuzzer() {
  isBuzzerActive = false;
  digitalWrite(BUZZER_PIN, LOW);
}

void updateBuzzer() {
  if (!isBuzzerActive || !buzzerEnabled || !isNewAlert) {
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  unsigned long currentTime = millis();
  bool critical = (getCriticalAlertCount() > 0);

  if (critical) {
    switch (buzzerState) {
      case 0:
        if (currentTime - lastBuzzerTime >= 150) {
          digitalWrite(BUZZER_PIN, LOW);
          buzzerState = 1;
          lastBuzzerTime = currentTime;
          beepCount++;
        } else {
          digitalWrite(BUZZER_PIN, HIGH);
        }
        break;
      case 1:
        if (currentTime - lastBuzzerTime >= 100) {
          digitalWrite(BUZZER_PIN, HIGH);
          buzzerState = 0;
          lastBuzzerTime = currentTime;
          if (beepCount >= 8) {
            buzzerState = 2;
            beepCount = 0;
          }
        } else {
          digitalWrite(BUZZER_PIN, LOW);
        }
        break;
      case 2:
        if (currentTime - lastBuzzerTime >= 800) {
          digitalWrite(BUZZER_PIN, LOW);
          buzzerState = 3;
          lastBuzzerTime = currentTime;
        } else {
          digitalWrite(BUZZER_PIN, HIGH);
        }
        break;
      case 3:
        if (currentTime - lastBuzzerTime >= 300) {
          digitalWrite(BUZZER_PIN, HIGH);
          buzzerState = 0;
          lastBuzzerTime = currentTime;
          beepCount = 0;
        } else {
          digitalWrite(BUZZER_PIN, LOW);
        }
        break;
    }
  } else {
    switch (buzzerState) {
      case 0:
        if (currentTime - lastBuzzerTime >= 300) {
          digitalWrite(BUZZER_PIN, LOW);
          buzzerState = 1;
          lastBuzzerTime = currentTime;
        } else {
          digitalWrite(BUZZER_PIN, HIGH);
        }
        break;
      case 1:
        if (currentTime - lastBuzzerTime >= 200) {
          digitalWrite(BUZZER_PIN, HIGH);
          buzzerState = 0;
          lastBuzzerTime = currentTime;
          beepCount++;
          if (beepCount >= 3) {
            beepCount = 0;
            buzzerState = 2;
            lastBuzzerTime = currentTime;
          }
        } else {
          digitalWrite(BUZZER_PIN, LOW);
        }
        break;
      case 2:
        if (currentTime - lastBuzzerTime >= 1000) {
          digitalWrite(BUZZER_PIN, HIGH);
          buzzerState = 0;
          lastBuzzerTime = currentTime;
        } else {
          digitalWrite(BUZZER_PIN, LOW);
        }
        break;
    }
  }
}

void displayCurrentAlertMedicine() {
  if (expiringCount == 0) return;
  
  Medicine currentMed = expiringMedicines[currentAlertIndex];
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.setTextSize(2);
  if (currentMed.daysUntilExpiry <= 1) {
    display.println("CRITICAL!");
    display.setTextSize(1);
    display.println("USE IMMEDIATELY!");
  } else if (currentMed.daysUntilExpiry <= 3) {
    display.println("ALERT!");
    display.setTextSize(1);
    display.println("EXPIRING SOON!");
  } else {
    display.println("Warning!");
    display.setTextSize(1);
    display.println("Check soon!");
  }
  
  display.println("---------------");
  
  String displayName = currentMed.name;
  if (displayName.length() > 16) {
    displayName = displayName.substring(0, 16);
  }
  display.setTextSize(1);
  display.print("Medicine: ");
  display.println(displayName);
  
  display.print("Expires in: ");
  display.print(currentMed.daysUntilExpiry);
  display.println(" days");
  
  display.print("Date: ");
  display.println(currentMed.expiryDate);
  
  display.println("---------------");
  if (expiringCount > 1) {
    display.print("(");
    display.print(currentAlertIndex + 1);
    display.print("/");
    display.print(expiringCount);
    display.println(") Next: 15s");
  } else {
    display.println("Rescan to clear");
  }
  
  display.print("Buzzer: ");
  display.println(buzzerEnabled ? "ON" : "OFF");
  
  display.display();
}

int getCriticalAlertCount() {
  int criticalCount = 0;
  DateTime now = getCorrectedTime();
  
  for (int i = 0; i < medicineCount; i++) {
    DateTime expiry = DateTime(currentMedicines[i].expiryDate.substring(0,4).toInt(), 
                             currentMedicines[i].expiryDate.substring(5,7).toInt(), 
                             currentMedicines[i].expiryDate.substring(8,10).toInt());
    
    int daysLeft = calculateDaysBetween(now, expiry);
    if (daysLeft <= 1 && daysLeft >= 0) {
      criticalCount++;
    }
  }
  return criticalCount;
}

int getActiveAlertCount() {
  int count = 0;
  DateTime now = getCorrectedTime();
  
  for (int i = 0; i < medicineCount; i++) {
    DateTime expiry = DateTime(currentMedicines[i].expiryDate.substring(0,4).toInt(), 
                             currentMedicines[i].expiryDate.substring(5,7).toInt(), 
                             currentMedicines[i].expiryDate.substring(8,10).toInt());
    
    int daysLeft = calculateDaysBetween(now, expiry);
    if (daysLeft <= 3 && daysLeft >= 0) {
      count++;
    }
  }
  return count;
}

String getMedicineStatus(int daysLeft) {
  if (daysLeft < 0) return "expired";
  if (daysLeft <= 1) return "critical";
  if (daysLeft <= 3) return "warning";
  return "good";
}

void displayMainScreen() {
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.setTextSize(1);
  display.println("Smart Medicine Fridge");
  display.println("---------------------");
  
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  
  display.print("Temp: ");
  display.print(temp, 1);
  display.println(" C");
  
  DateTime now = getCorrectedTime();
  display.print("Time: ");
  display.print(now.hour(), DEC);
  display.print(':');
  if(now.minute() < 10) display.print('0');
  display.print(now.minute(), DEC);
  
  display.print(" WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println("ON");
  } else {
    display.println("OFF");
  }
  
  display.print("Meds: ");
  display.println(medicineCount);
  display.println("---------------------");
  
  if (alertActive) {
    display.setTextSize(1);
    display.println("!! ALERT ACTIVE !!");
    display.print(expiringCount);
    display.println(" meds expiring!");
    
    if (expiringCount > 0) {
      display.print("Current: ");
      String currentMed = expiringMedicines[currentAlertIndex].name;
      if (currentMed.length() > 10) {
        currentMed = currentMed.substring(0, 10) + "...";
      }
      display.println(currentMed);
    }
  } else if (medicineCount > 0) {
    display.println("System: NORMAL");
    display.print("Last: ");
    String lastName = currentMedicines[medicineCount-1].name;
    if (lastName.length() > 12) {
      lastName = lastName.substring(0, 12) + "...";
    }
    display.println(lastName);
  } else {
    display.println("Scan RFID to");
    display.println("add medicine");
  }
  
  display.println("---------------------");
  if (buzzerEnabled) {
    display.println("Buzzer: ON");
  } else {
    display.println("Buzzer: OFF");
  }
  
  display.display();
}

void displayMedicineDetails(String name, String expiry, int daysLeft, String action) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(action);
  display.setTextSize(1);
  display.println("---------------");
  display.print("Med: ");
  display.println(name);
  display.print("Expiry: ");
  display.println(expiry);
  display.print("Days Left: ");
  display.println(daysLeft);
  display.println("---------------");
  
  if (daysLeft <= 1 && daysLeft >= 0) {
    display.println("!! USE IMMEDIATELY !!");
  } else if (daysLeft <= 3 && daysLeft >= 0) {
    display.println("!! EXPIRING SOON !!");
  } else if (daysLeft < 0) {
    display.println("!! ALREADY EXPIRED !!");
  }
  
  display.display();
  delay(3000);
}

void displayMedicineRemoved(String medicineName) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("REMOVED");
  display.setTextSize(1);
  display.println("---------------");
  display.print("Medicine: ");
  display.println(medicineName);
  display.println("---------------");
  display.println("Taken OUT of");
  display.println("fridge");
  display.display();
  delay(3000);
}

void displayUnknownTag(String uid) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("UNKNOWN TAG");
  display.println("---------------");
  display.print("UID: ");
  display.println(uid);
  display.println("---------------");
  display.println("Not in database");
  display.display();
  delay(3000);
}

void handleRFIDScan() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (i > 0) uid += " ";
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uid += "0";
    }
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  
  Serial.print("Scanned RFID UID: ");
  Serial.println(uid);
  
  String normalizedUID = normalizeUID(uid);
  
  String medicineName = "Unknown Medicine";
  String expiryDate = "2024-12-31";
  bool found = false;
  
  for (int i = 0; i < numMedicines; i++) {
    String knownUID = normalizeUID(knownRFIDs[i][0]);
    if (normalizedUID == knownUID) {
      medicineName = knownRFIDs[i][1];
      expiryDate = knownRFIDs[i][2];
      found = true;
      Serial.print("MATCH FOUND: ");
      Serial.println(medicineName);
      break;
    }
  }
  
  if (!found) {
    Serial.println("Unknown RFID tag!");
    displayUnknownTag(uid);
    mfrc522.PICC_HaltA();
    return;
  }
  
  bool medicineInFridge = false;
  int medicineIndex = -1;
  
  for (int i = 0; i < medicineCount; i++) {
    if (currentMedicines[i].name == medicineName) {
      medicineInFridge = true;
      medicineIndex = i;
      break;
    }
  }
  
  if (medicineInFridge) {
    removeMedicine(medicineName, medicineIndex);
  } else {
    addMedicine(medicineName, expiryDate);
  }
  
  mfrc522.PICC_HaltA();
}

// CORRECTED: Add medicine with proper days calculation
void addMedicine(String medicineName, String expiryDate) {
  int daysUntilExpiry = getDaysUntilExpiry(expiryDate);
  
  if (medicineCount < 10) {
    currentMedicines[medicineCount].name = medicineName;
    currentMedicines[medicineCount].expiryDate = expiryDate;
    currentMedicines[medicineCount].daysUntilExpiry = daysUntilExpiry;
    medicineCount++;
    
    Serial.print("ADDED to fridge: ");
    Serial.print(medicineName);
    Serial.print(" - Expires: ");
    Serial.print(expiryDate);
    Serial.print(" (");
    Serial.print(daysUntilExpiry);
    Serial.println(" days)");
    
    sendInventoryUpdate();
  }
  
  resetAlertSystem();
  displayMedicineDetails(medicineName, expiryDate, daysUntilExpiry, "ADDED");
}

void removeMedicine(String medicineName, int index) {
  for (int i = index; i < medicineCount - 1; i++) {
    currentMedicines[i] = currentMedicines[i + 1];
  }
  medicineCount--;
  
  Serial.print("REMOVED from fridge: ");
  Serial.println(medicineName);
  
  sendInventoryUpdate();
  
  resetAlertSystem();
  displayMedicineRemoved(medicineName);
}

void resetAlertSystem() {
  alertActive = false;
  stopBuzzer();
  lastAlertTime = millis();
  currentAlertIndex = 0;
  isNewAlert = true;
  
  Serial.println("Alert system reset");
  checkExpiryAlerts();
}

// CORRECTED: Check expiry alerts with proper days calculation
void checkExpiryAlerts() {
  bool foundAlert = false;
  DateTime now = getCorrectedTime();
  
  expiringCount = 0;
  
  for (int i = 0; i < medicineCount; i++) {
    // Recalculate days left for each medicine
    currentMedicines[i].daysUntilExpiry = getDaysUntilExpiry(currentMedicines[i].expiryDate);
    
    Serial.print("Medicine: ");
    Serial.print(currentMedicines[i].name);
    Serial.print(" - Days left: ");
    Serial.println(currentMedicines[i].daysUntilExpiry);
    
    if (currentMedicines[i].daysUntilExpiry <= 3 && currentMedicines[i].daysUntilExpiry >= 0) {
      foundAlert = true;
      if (expiringCount < 10) {
        expiringMedicines[expiringCount] = currentMedicines[i];
        expiringCount++;
      }
    }
  }
  
  // Sort by days left (soonest first)
  for (int i = 0; i < expiringCount - 1; i++) {
    for (int j = i + 1; j < expiringCount; j++) {
      if (expiringMedicines[i].daysUntilExpiry > expiringMedicines[j].daysUntilExpiry) {
        Medicine temp = expiringMedicines[i];
        expiringMedicines[i] = expiringMedicines[j];
        expiringMedicines[j] = temp;
      }
    }
  }
  
  if (foundAlert && !alertActive) {
    isNewAlert = true;
    alertStartTime = millis();
    lastAlertChange = millis();
    currentAlertIndex = 0;
    displayCurrentAlertMedicine();
    startBuzzerAlert(getCriticalAlertCount() > 0);
  } else if (foundAlert && alertActive) {
    // Alert already active
  }
  
  alertActive = foundAlert;
  if (alertActive) {
    lastAlertTime = millis();
    if (buzzerEnabled && isNewAlert) {
      Serial.print("*** NEW ALERT ACTIVE - ");
      Serial.print(expiringCount);
      Serial.println(" medicines expiring ***");
    } else {
      Serial.print("*** EXISTING ALERT - ");
      Serial.print(expiringCount);
      Serial.println(" medicines expiring ***");
    }
  } else {
    stopBuzzer();
    isNewAlert = true;
    Serial.println("--- No alerts - System clear ---");
  }
}

// ================== CORS HEADER FUNCTION ==================
void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendInventoryUpdate() {
  Serial.println("Inventory updated - web clients notified");
}

// ================== WEB SERVER SETUP ==================
void setupWebServer() {
  // ========== API ENDPOINTS ==========
  
  // 1. Get sensor data
  server.on("/api/sensors", HTTP_GET, []() {
    Serial.println("GET /api/sensors");
    
    StaticJsonDocument<256> doc;
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    
    doc["temperature"] = temp;
    doc["humidity"] = 60;
    doc["activeAlerts"] = getActiveAlertCount();
    doc["medicineCount"] = medicineCount;
    doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    
    String response;
    serializeJson(doc, response);
    
    addCorsHeaders();
    server.send(200, "application/json", response);
  });
  
  // 2. Get inventory
  server.on("/api/inventory", HTTP_GET, []() {
    Serial.println("GET /api/inventory");
    
    StaticJsonDocument<2048> doc;
    JsonArray medicinesArray = doc.createNestedArray("medicines");
    
    for (int i = 0; i < medicineCount; i++) {
      JsonObject med = medicinesArray.createNestedObject();
      med["id"] = i + 1;
      med["name"] = currentMedicines[i].name;
      med["expiry"] = currentMedicines[i].expiryDate;
      med["daysLeft"] = currentMedicines[i].daysUntilExpiry;
      med["status"] = getMedicineStatus(currentMedicines[i].daysUntilExpiry);
      
      if (currentMedicines[i].daysUntilExpiry <= 1) {
        med["alertLevel"] = "critical";
      } else if (currentMedicines[i].daysUntilExpiry <= 3) {
        med["alertLevel"] = "warning";
      } else {
        med["alertLevel"] = "normal";
      }
    }
    
    doc["count"] = medicineCount;
    doc["totalCapacity"] = 10;
    doc["alerts"] = getActiveAlertCount();
    
    String response;
    serializeJson(doc, response);
    
    addCorsHeaders();
    server.send(200, "application/json", response);
  });
  
  // 3. Add medicine from web app
  server.on("/api/add", HTTP_POST, []() {
    Serial.println("POST /api/add");
    
    if (server.hasArg("plain")) {
      StaticJsonDocument<256> reqDoc;
      DeserializationError error = deserializeJson(reqDoc, server.arg("plain"));
      
      if (error) {
        addCorsHeaders();
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      
      String name = reqDoc["name"] | "";
      String expiry = reqDoc["expiry"] | "";
      String rfid = reqDoc["rfid"] | "";
      
      if (name == "" || expiry == "") {
        addCorsHeaders();
        server.send(400, "application/json", "{\"error\":\"Name and expiry required\"}");
        return;
      }
      
      if (rfid != "" && rfid != "MANUAL" && numMedicines < MAX_MEDICINES) {
        knownRFIDs[numMedicines][0] = rfid;
        knownRFIDs[numMedicines][1] = name;
        knownRFIDs[numMedicines][2] = expiry;
        numMedicines++;
        Serial.print("Added to database: ");
        Serial.print(name);
        Serial.print(" with RFID: ");
        Serial.println(rfid);
      }
      
      addMedicine(name, expiry);
      
      StaticJsonDocument<128> respDoc;
      respDoc["status"] = "success";
      respDoc["message"] = "Medicine added successfully";
      respDoc["count"] = medicineCount;
      
      String response;
      serializeJson(respDoc, response);
      
      addCorsHeaders();
      server.send(200, "application/json", response);
      
    } else {
      addCorsHeaders();
      server.send(400, "application/json", "{\"error\":\"No data received\"}");
    }
  });
  
  // OPTIONS for CORS preflight
  server.on("/api/add", HTTP_OPTIONS, []() {
    addCorsHeaders();
    server.send(204);
  });
  
  // 4. Get all known RFID tags
  server.on("/api/rfids", HTTP_GET, []() {
    Serial.println("GET /api/rfids");
    
    StaticJsonDocument<1024> doc;
    JsonArray rfids = doc.createNestedArray("rfids");
    
    for (int i = 0; i < numMedicines; i++) {
      JsonObject rfid = rfids.createNestedObject();
      rfid["uid"] = knownRFIDs[i][0];
      rfid["name"] = knownRFIDs[i][1];
      rfid["expiry"] = knownRFIDs[i][2];
    }
    
    doc["count"] = numMedicines;
    
    String response;
    serializeJson(doc, response);
    
    addCorsHeaders();
    server.send(200, "application/json", response);
  });
  
  // 5. Check system status
  server.on("/api/status", HTTP_GET, []() {
    Serial.println("GET /api/status");
    
    StaticJsonDocument<512> doc;
    doc["system"] = "Smart Medicine Fridge";
    doc["version"] = "1.0";
    doc["wifiStatus"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
    doc["ip"] = WiFi.localIP().toString();
    doc["uptime"] = millis() / 1000;
    
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    doc["temperature"] = temp;
    
    doc["activeAlerts"] = getActiveAlertCount();
    doc["medicineCount"] = medicineCount;
    
    // Add RTC time for debugging
    DateTime now = getCorrectedTime();
    char timeStr[20];
    sprintf(timeStr, "%04d-%02d-%02d %02d:%02d", 
            now.year(), now.month(), now.day(), now.hour(), now.minute());
    doc["rtcTime"] = timeStr;
    
    String response;
    serializeJson(doc, response);
    
    addCorsHeaders();
    server.send(200, "application/json", response);
  });
  
  // 6. Force RFID scan
  server.on("/api/scan", HTTP_GET, []() {
    Serial.println("GET /api/scan");
    
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      handleRFIDScan();
      String response = "{\"status\":\"scanned\",\"message\":\"RFID card scanned successfully\"}";
      
      addCorsHeaders();
      server.send(200, "application/json", response);
    } else {
      String response = "{\"status\":\"no_card\",\"message\":\"No RFID card detected\"}";
      
      addCorsHeaders();
      server.send(200, "application/json", response);
    }
  });
  
  // 7. Clear all medicines
  server.on("/api/clear", HTTP_POST, []() {
    Serial.println("POST /api/clear");
    
    medicineCount = 0;
    alertActive = false;
    stopBuzzer();
    
    StaticJsonDocument<128> doc;
    doc["status"] = "success";
    doc["message"] = "All medicines cleared";
    doc["count"] = 0;
    
    String response;
    serializeJson(doc, response);
    
    addCorsHeaders();
    server.send(200, "application/json", response);
    
    Serial.println("All medicines cleared via web API");
  });
  
  // OPTIONS for /api/clear
  server.on("/api/clear", HTTP_OPTIONS, []() {
    addCorsHeaders();
    server.send(204);
  });
  
  // OPTIONS handler for all other routes
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      addCorsHeaders();
      server.send(204);
    } else {
      server.send(404, "text/plain", "404: Not Found");
    }
  });
  
  // Simple homepage
  server.on("/", HTTP_GET, []() {
    Serial.println("GET /");
    
    String html = "<!DOCTYPE html><html><head><title>Medicine Fridge</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;padding:20px;max-width:800px;margin:0 auto;}";
    html += ".card{background:#f5f5f5;padding:20px;border-radius:10px;margin:10px 0;}";
    html += ".button{padding:10px 20px;background:#007bff;color:white;border:none;border-radius:5px;cursor:pointer;}</style>";
    html += "</head><body>";
    html += "<h1>Smart Medicine Fridge</h1>";
    html += "<div class='card'>";
    html += "<h3>System Status</h3>";
    html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>Medicines in Fridge:</strong> " + String(medicineCount) + "</p>";
    html += "<p><strong>Active Alerts:</strong> " + String(getActiveAlertCount()) + "</p>";
    html += "</div>";
    html += "<div class='card'>";
    html += "<h3>API Endpoints</h3>";
    html += "<p><a href='/api/sensors' target='_blank'>/api/sensors</a> - Sensor data</p>";
    html += "<p><a href='/api/inventory' target='_blank'>/api/inventory</a> - Medicine inventory</p>";
    html += "<p><a href='/api/status' target='_blank'>/api/status</a> - System status</p>";
    html += "<p><a href='/api/rfids' target='_blank'>/api/rfids</a> - Known RFID tags</p>";
    html += "</div>";
    html += "<p>Use the web dashboard for full interface.</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
  });
  
  // Handle 404
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
  
  server.begin();
  Serial.println("✅ Web server started!");
  Serial.print("📱 Open browser to: http://");
  Serial.println(WiFi.localIP());
}

void connectToWiFi() {
  Serial.println();
  Serial.print("📶 Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Connecting to");
  display.println("WiFi...");
  display.display();
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("📡 IP address: ");
    Serial.println(WiFi.localIP());
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("✅ WiFi Connected!");
    display.print("IP: ");
    display.println(WiFi.localIP().toString());
    display.println("---------------------");
    display.println("Web Server Ready!");
    display.display();
    delay(2000);
  } else {
    Serial.println("\n❌ WiFi connection failed!");
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("❌ WiFi FAILED!");
    display.println("Check credentials");
    display.display();
    delay(3000);
  }
}

// ================== DEBUG TIME FUNCTION ==================
void debugTimeCalculations() {
  Serial.println("\n🔍 DEBUG: Time Calculations");
  
  DateTime now = getCorrectedTime();
  Serial.print("Current corrected time: ");
  Serial.print(now.year());
  Serial.print("-");
  Serial.print(now.month());
  Serial.print("-");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.println(now.minute());
  
  // Test Nutrolin-B calculation
  String expiryDate = "2025-12-28";
  int daysLeft = getDaysUntilExpiry(expiryDate);
  
  Serial.print("Days until Nutrolin-B (2025-12-28): ");
  Serial.println(daysLeft);
  
  if (daysLeft > 1000 || daysLeft < -1000) {
    Serial.println("⚠ WARNING: Time calculation seems incorrect!");
    Serial.println("Check RTC battery and time setting.");
  }
}

// ================== SETUP & LOOP ==================

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 Smart Medicine Fridge Starting...");
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("❌ SSD1306 allocation failed"));
    for(;;);
  }
  
  // Initialize RFID
  SPI.begin();
  mfrc522.PCD_Init();
  
  // Initialize Temperature Sensor
  sensors.begin();
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("❌ Couldn't find RTC");
    while (1);
  }
  
  // FIXED: Always set RTC to compile time (with 30 min correction)
  DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
  
  // Add 30 minutes correction
  int correctedMinute = compileTime.minute() + 30;
  int correctedHour = compileTime.hour();
  int correctedDay = compileTime.day();
  int correctedMonth = compileTime.month();
  int correctedYear = compileTime.year();
  
  if (correctedMinute >= 60) {
    correctedMinute -= 60;
    correctedHour += 1;
  }
  if (correctedHour >= 24) {
    correctedHour -= 24;
    correctedDay += 1;
  }
  
  // Always set RTC time (not just when power lost)
  rtc.adjust(DateTime(correctedYear, correctedMonth, correctedDay, 
                      correctedHour, correctedMinute, compileTime.second()));
  
  Serial.print("📅 RTC set to: ");
  DateTime now = rtc.now();
  Serial.print(now.year());
  Serial.print("-");
  Serial.print(now.month());
  Serial.print("-");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());
  
  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize Buzzer Switch
  pinMode(BUZZER_SWITCH, INPUT_PULLUP);
  
  // Initial display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Smart Medical Fridge");
  display.println("Initializing...");
  display.display();
  delay(1000);
  
  // Connect to WiFi
  connectToWiFi();
  
  // Debug time calculations
  debugTimeCalculations();
  
  // Start web server
  setupWebServer();
  
  Serial.println("\n📋 Known RFID Tags:");
  for (int i = 0; i < numMedicines; i++) {
    Serial.print("  Tag ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(knownRFIDs[i][0]);
    Serial.print(" -> ");
    Serial.println(knownRFIDs[i][1]);
  }
  
  Serial.println("\n✅ System Ready!");
  Serial.println("=======================");
}

void loop() {
  // Check BUZZER SWITCH status
  bool previousBuzzerState = buzzerEnabled;
  buzzerEnabled = (digitalRead(BUZZER_SWITCH) == HIGH);
  
  if (previousBuzzerState && !buzzerEnabled) {
    stopBuzzer();
    Serial.println("🔇 BUZZER DISABLED - Switch is OFF");
  }
  
  if (!previousBuzzerState && buzzerEnabled) {
    Serial.println("🔊 BUZZER ENABLED - Switch is ON");
    isNewAlert = false;
  }
  
  // Handle alert cycling
  if (alertActive && expiringCount > 0) {
    if (millis() - lastAlertChange > ALERT_DISPLAY_TIME) {
      currentAlertIndex = (currentAlertIndex + 1) % expiringCount;
      lastAlertChange = millis();
      displayCurrentAlertMedicine();
    }
  }
  
  // Display main screen unless alert is active
  if (!alertActive) {
    displayMainScreen();
  }
  
  // Update non-blocking buzzer
  if (alertActive && buzzerEnabled && isNewAlert) {
    updateBuzzer();
  } else {
    stopBuzzer();
  }
  
  // Check for new RFID cards
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    handleRFIDScan();
    delay(1000);
  }
  
  // Check expiry alerts every 10 seconds
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    checkExpiryAlerts();
  }
  
  // Auto-stop buzzer after 5 minutes
  if (alertActive && (millis() - alertStartTime > MAX_ALERT_TIME)) {
    stopBuzzer();
    Serial.println("⏰ Buzzer auto-stopped after 5 minutes");
  }
  
  // Handle web client requests
  server.handleClient();
  
  delay(50);
}