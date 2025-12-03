#ifndef HUNTER_ESP32_H
#define HUNTER_ESP32_H

#include <vector>
#include <Arduino.h>

// This makes inverting the signal easy
#define HUNTER_ONE HIGH
#define HUNTER_ZERO LOW

#define START_INTERVAL 900
#define SHORT_INTERVAL 208
#define LONG_INTERVAL 1875

#define HUNTER_PIN 12 // GPIO12 - same as ESP8266 for compatibility
// #define ENABLE_PIN 17 // GPIO17 - if needed
// #define LED_PIN 2     // GPIO2 - Built-in LED on ESP32

// Function declarations
void HunterStop(byte zone);
void HunterStart(byte zone, byte time);
void HunterProgram(byte num);
void HunterBitfield(std::vector <byte>& bits, byte pos, byte val, byte len);
void HunterWrite(std::vector <byte> buffer, bool extrabit);
void HunterLow(void);
void HunterHigh(void);

#endif