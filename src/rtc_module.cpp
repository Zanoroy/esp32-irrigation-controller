#include "rtc_module.h"
#include <WiFi.h>
#include <time.h>

// Constructor
RTCModule::RTCModule() : rtcInitialized(false) {
}

// Initialize the RTC
bool RTCModule::begin() {
    Serial.println("Initializing RTC module...");

    // Initialize I2C with specific pins for TinyRTC if not already started
    // The Wire library will handle this gracefully if already initialized
    Wire.begin(SDA_PIN, SCL_PIN);

    // Small delay to ensure I2C is ready
    delay(100);

    // Scan I2C bus to see what devices are present
    Serial.println("Scanning I2C bus...");
    byte devicesFound = 0;
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("  I2C device found at 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            devicesFound++;
        }
    }

    if (devicesFound == 0) {
        Serial.println("  No I2C devices found!");
        Serial.println("  Check wiring:");
        Serial.println("    SDA → GPIO21");
        Serial.println("    SCL → GPIO22");
        Serial.println("    Power and Ground connections");
    } else {
        Serial.println("  Scan complete. Found " + String(devicesFound) + " device(s)");
    }

    // Initialize RTC, explicitly passing the Wire object
    if (!rtc.begin(&Wire)) {
        Serial.println("ERROR: Couldn't find RTC! Check wiring:");
        Serial.println("  TinyRTC VCC → ESP32 5V (or 3.3V)");
        Serial.println("  TinyRTC GND → ESP32 GND");
        Serial.println("  TinyRTC SDA → ESP32 GPIO21");
        Serial.println("  TinyRTC SCL → ESP32 GPIO22");
        rtcInitialized = false;
        return false;
    }

    Serial.println("RTC found and initialized");

    // Check if RTC lost power and needs time setting
    if (rtc.lostPower()) {
        Serial.println("WARNING: RTC lost power!");
        Serial.println("RTC needs time setting");

        // Try to sync with NTP if WiFi is connected
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Attempting to sync RTC with NTP time...");
            if (syncWithNTP()) {
                Serial.println("RTC time synchronized with NTP");
            } else {
                Serial.println("Failed to sync with NTP, using compile time");
                rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            }
        } else {
            Serial.println("No WiFi connection, setting RTC to compile time");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    } else {
        Serial.println("RTC is running and keeping time");
    }

    // Note: EEPROM removed - using SPIFFS/NVS for storage instead

    rtcInitialized = true;
    printStatus();
    return true;
}

// Get current time from RTC
DateTime RTCModule::getCurrentTime() {
    if (!rtcInitialized) {
        Serial.println("ERROR: RTC not initialized");
        return DateTime(2000, 1, 1, 0, 0, 0); // Return epoch time if not initialized
    }
    return rtc.now();
}

// Set time manually
bool RTCModule::setTime(const DateTime& time) {
    if (!rtcInitialized) {
        Serial.println("ERROR: RTC not initialized");
        return false;
    }

    rtc.adjust(time);
    Serial.println("RTC time set to: " + getDateTimeString());
    return true;
}

// Set time with individual components
bool RTCModule::setTime(int year, int month, int day, int hour, int minute, int second) {
    DateTime time(year, month, day, hour, minute, second);
    return setTime(time);
}

// Get formatted time string (HH:MM:SS)
String RTCModule::getTimeString() {
    if (!rtcInitialized) return "00:00:00";

    DateTime now = getCurrentTime();
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    return String(timeStr);
}

// Get formatted date string (YYYY-MM-DD)
String RTCModule::getDateString() {
    if (!rtcInitialized) return "2000-01-01";

    DateTime now = getCurrentTime();
    char dateStr[11];
    sprintf(dateStr, "%04d-%02d-%02d", now.year(), now.month(), now.day());
    return String(dateStr);
}

// Get formatted date and time string
String RTCModule::getDateTimeString() {
    if (!rtcInitialized) return "2000-01-01 00:00:00";

    return getDateString() + " " + getTimeString();
}

// Check if RTC is running
bool RTCModule::isRunning() {
    if (!rtcInitialized) return false;
    return !rtc.lostPower();  // lostPower() returns true if RTC lost power, so invert
}

// Sync RTC with NTP time
bool RTCModule::syncWithNTP() {
    return syncWithNTP("pool.ntp.org", "time.nist.gov", 0);
}

// Sync RTC with NTP time using configurable servers and timezone
bool RTCModule::syncWithNTP(const char* server1, const char* server2, int timezoneOffset) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, cannot sync with NTP");
        return false;
    }

    Serial.printf("Configuring NTP with servers: %s, %s (UTC%+d)\n", server1, server2, timezoneOffset);

    // Configure timezone offset in seconds
    long offsetSeconds = timezoneOffset * 3600;
    configTime(offsetSeconds, 0, server1, server2);

    // Wait for NTP sync (timeout after 10 seconds)
    int timeout = 0;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) && timeout < 10) {
        delay(1000);
        timeout++;
        Serial.print(".");
    }
    Serial.println();

    if (timeout >= 10) {
        Serial.println("Failed to get NTP time");
        return false;
    }

    // Convert to DateTime and set RTC (store as UTC in RTC)
    // The getLocalTime already includes timezone offset, so we need to convert back to UTC
    DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Convert local time back to UTC for RTC storage
    DateTime utcTime = DateTime(ntpTime.unixtime() - offsetSeconds);
    rtc.adjust(utcTime);

    Serial.printf("RTC synchronized with NTP time (UTC): %s\n", getDateTimeString().c_str());
    return true;
}

// Print RTC status information
void RTCModule::printStatus() {
    if (!rtcInitialized) {
        Serial.println("RTC Status: Not initialized");
        return;
    }

    Serial.println("=== RTC Status ===");
    Serial.println("RTC Type: DS3231");
    Serial.println("I2C Pins: SDA=GPIO21, SCL=GPIO22");
    Serial.println("Running: " + String(isRunning() ? "Yes" : "No"));
    Serial.println("Current Time: " + getDateTimeString());

    DateTime now = getCurrentTime();
    Serial.println("Unix Timestamp: " + String(now.unixtime()));

    // Day of week
    String days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    Serial.println("Day of Week: " + days[now.dayOfTheWeek()]);
    Serial.println("==================");
}