#include "rtc_module.h"
#include <WiFi.h>
#include <time.h>

// Constructor
RTCModule::RTCModule() : rtcInitialized(false), eepromAvailable(false) {
}

// Initialize the RTC
bool RTCModule::begin() {
    Serial.println("Initializing RTC module...");

    // Initialize I2C with specific pins for TinyRTC
    Wire.begin(SDA_PIN, SCL_PIN);

    // Initialize RTC
    if (!rtc.begin()) {
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
    if (!rtc.isrunning()) {
        Serial.println("WARNING: RTC is NOT running!");
        Serial.println("RTC may need new battery or time setting");

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

    // Test for EEPROM availability
    Serial.println("Testing for AT24C32 EEPROM...");
    if (testEEPROM()) {
        Serial.println("AT24C32 EEPROM detected and functional");
        eepromAvailable = true;
    } else {
        Serial.println("AT24C32 EEPROM not detected or not functional");
        eepromAvailable = false;
    }

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
    return rtc.isrunning();
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
    Serial.println("RTC Type: DS1307 (TinyRTC)");
    Serial.println("EEPROM: AT24C32 " + String(eepromAvailable ? "Available" : "Not Available"));
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

// Write single byte to EEPROM
bool RTCModule::writeEEPROM(uint16_t address, uint8_t data) {
    if (!eepromAvailable) return false;

    Wire.beginTransmission(EEPROM_I2C_ADDRESS);
    Wire.write((address >> 8) & 0xFF); // High byte of address
    Wire.write(address & 0xFF);        // Low byte of address
    Wire.write(data);

    uint8_t result = Wire.endTransmission();
    if (result == 0) {
        delay(5); // Write cycle time for AT24C32
        return true;
    }
    return false;
}

// Write multiple bytes to EEPROM
bool RTCModule::writeEEPROM(uint16_t address, const uint8_t* data, size_t length) {
    if (!eepromAvailable) return false;

    for (size_t i = 0; i < length; i++) {
        if (!writeEEPROM(address + i, data[i])) {
            return false;
        }
    }
    return true;
}

// Read single byte from EEPROM
uint8_t RTCModule::readEEPROM(uint16_t address) {
    if (!eepromAvailable) return 0xFF;

    Wire.beginTransmission(EEPROM_I2C_ADDRESS);
    Wire.write((address >> 8) & 0xFF); // High byte of address
    Wire.write(address & 0xFF);        // Low byte of address

    if (Wire.endTransmission() == 0) {
        Wire.requestFrom(EEPROM_I2C_ADDRESS, 1);
        if (Wire.available()) {
            return Wire.read();
        }
    }
    return 0xFF; // Error value
}

// Read multiple bytes from EEPROM
bool RTCModule::readEEPROM(uint16_t address, uint8_t* buffer, size_t length) {
    if (!eepromAvailable) return false;

    for (size_t i = 0; i < length; i++) {
        buffer[i] = readEEPROM(address + i);
        if (buffer[i] == 0xFF && address + i != 0xFFFF) {
            // Might be an error (unless we're actually reading 0xFF)
        }
    }
    return true;
}

// Test EEPROM functionality
bool RTCModule::testEEPROM() {
    // Test address for EEPROM detection
    uint16_t testAddress = 0x0000;
    uint8_t testValue = 0xAA;
    uint8_t originalValue;

    // Try to communicate with EEPROM
    Wire.beginTransmission(EEPROM_I2C_ADDRESS);
    uint8_t result = Wire.endTransmission();

    if (result != 0) {
        Serial.println("EEPROM not responding on I2C address 0x57");
        return false;
    }

    // Read original value
    originalValue = readEEPROM(testAddress);

    // Write test value
    Wire.beginTransmission(EEPROM_I2C_ADDRESS);
    Wire.write((testAddress >> 8) & 0xFF);
    Wire.write(testAddress & 0xFF);
    Wire.write(testValue);
    result = Wire.endTransmission();

    if (result != 0) {
        Serial.println("EEPROM write failed");
        return false;
    }

    delay(5); // Write cycle time

    // Read back test value
    uint8_t readValue = readEEPROM(testAddress);

    // Restore original value
    Wire.beginTransmission(EEPROM_I2C_ADDRESS);
    Wire.write((testAddress >> 8) & 0xFF);
    Wire.write(testAddress & 0xFF);
    Wire.write(originalValue);
    Wire.endTransmission();
    delay(5);

    // Check if test was successful
    if (readValue == testValue) {
        Serial.println("EEPROM test successful (4KB AT24C32)");
        return true;
    } else {
        Serial.println("EEPROM test failed - read/write mismatch");
        return false;
    }
}