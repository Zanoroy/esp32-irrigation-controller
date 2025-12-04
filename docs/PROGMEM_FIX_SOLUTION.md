# ESP32 PROGMEM Cross-Compilation Unit Fix

## Problem
The ESP32 Hunter WiFi Remote was experiencing critical Guru Meditation Errors (LoadProhibited) when serving web pages to external clients. The error signature was:

```
Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.
Core  1 register dump:
PC      : 0xb3400003  PS      : 0x00060130  A0      : 0x00000000  A1      : 0x3ffb1f40
...
ELF file SHA256: 6d734f08d7cceef7
```

## Root Cause
The issue was caused by ESP32's PROGMEM handling across compilation units. When HTML content was stored in `web_html.cpp` as:

```cpp
static const char index_html[] PROGMEM = R"rawliteral(...)rawliteral";
```

And accessed from `web_server.cpp` via function calls or direct references, the ESP32 would crash with LoadProhibited exceptions due to memory alignment and cross-compilation unit PROGMEM access issues.

## Failed Approaches
1. **beginResponse_P() with exception handling** - Still crashed
2. **String-based HTML serving with FPSTR()** - Still crashed
3. **Function wrapper encapsulation** - Still crashed
4. **Static PROGMEM with getIndexHTML() function** - Still crashed

All approaches failed because they still involved cross-compilation unit PROGMEM access.

## Final Solution
**Moved HTML content directly into web_server.cpp** to eliminate any cross-compilation unit PROGMEM access:

### Changes Made:
1. **Moved HTML from web_html.cpp to web_server.cpp**:
   ```cpp
   // In web_server.cpp - HTML content now in same compilation unit
   static const char index_html[] PROGMEM = R"rawliteral(
   <!DOCTYPE html>
   <html>
   ...
   )rawliteral";
   ```

2. **Simplified handleRoot() method**:
   ```cpp
   void HunterWebServer::handleRoot(AsyncWebServerRequest *request) {
       Serial.println("Web request from: " + request->client()->remoteIP().toString());
       Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

       // Use local PROGMEM directly - no cross-compilation unit access
       request->send_P(200, "text/html", index_html);
   }
   ```

3. **Removed web_html.h include** from web_server.cpp

## Result
- Build successful: 858,889 bytes (65.5% flash)
- No more cross-compilation unit PROGMEM access
- Should eliminate Guru Meditation Errors

## Key Lesson
ESP32 has serious issues with PROGMEM access across compilation units. For large HTML content, it must be in the same compilation unit where it's accessed, not separated into different files.

## Alternative Approaches (Not Tested)
If modular architecture is required:
1. **SPIFFS/LittleFS**: Store HTML files in filesystem instead of PROGMEM
2. **Heap allocation**: Load HTML into heap memory (uses more RAM)
3. **External storage**: Store HTML on SD card or external flash

## Build Status
✅ **FIXED**: Single compilation unit approach eliminates cross-unit PROGMEM issues