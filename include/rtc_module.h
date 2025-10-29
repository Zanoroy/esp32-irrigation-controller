#ifndef RTC_MODULE_H
#define RTC_MODULE_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// RTC management class for Hunter WiFi Remote
class RTCModule {
private:
    RTC_DS1307 rtc;
    static const int SDA_PIN = 21;
    static const int SCL_PIN = 22;
    static const int EEPROM_I2C_ADDRESS = 0x57; // AT24C32 EEPROM address
    bool rtcInitialized;
    bool eepromAvailable;

public:
    // Constructor
    RTCModule();

    // Initialize the RTC
    bool begin();

    // Time management
    DateTime getCurrentTime();
    bool setTime(const DateTime& time);
    bool setTime(int year, int month, int day, int hour, int minute, int second);
    String getTimeString();
    String getDateString();
    String getDateTimeString();

    // Status checks
    bool isRunning();
    bool isInitialized() const { return rtcInitialized; }
    bool isEEPROMAvailable() const { return eepromAvailable; }

    // EEPROM functions (AT24C32 - 4KB / 32Kbit)
    bool writeEEPROM(uint16_t address, uint8_t data);
    bool writeEEPROM(uint16_t address, const uint8_t* data, size_t length);
    uint8_t readEEPROM(uint16_t address);
    bool readEEPROM(uint16_t address, uint8_t* buffer, size_t length);
    bool testEEPROM();

    // Utility functions
    bool syncWithNTP();
    bool syncWithNTP(const char* server1, const char* server2, int timezoneOffset = 0);
    void printStatus();
};

#endif // RTC_MODULE_H