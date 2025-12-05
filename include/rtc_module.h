#ifndef RTC_MODULE_H
#define RTC_MODULE_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// RTC management class for Hunter WiFi Remote
class RTCModule {
private:
    RTC_DS3231 rtc;
    static const int SDA_PIN = 21;
    static const int SCL_PIN = 22;
    bool rtcInitialized;

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

    // Utility functions
    bool syncWithNTP();
    bool syncWithNTP(const char* server1, const char* server2, int timezoneOffset = 0);
    void printStatus();
};

#endif // RTC_MODULE_H