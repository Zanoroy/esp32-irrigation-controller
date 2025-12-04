# Hunter WiFi Remote Control - ESP32 Project

A WiFi-based remote control for Hunter X-Core sprinkler controllers, adapted for ESP32 D1 Mini with PlatformIO.

## ✅ Project Status: Ready to Use

This project is fully configured and tested for:
- **ESP32 D1 Mini / ESP-WROOM-32** (primary target)
- **ESP32-S2** (compatible)
- **ESP32-S3** (compatible)

## 🚀 Quick Start

### Prerequisites
- **ESP32 D1 Mini** or compatible board
- **USB cable** (data cable, not charge-only)
- **PlatformIO Core** (already configured)
- **VS Code** with PlatformIO extension (optional but recommended)

### Build and Upload

**Option 1: Use Development Helper (Easiest)**
```bash
./dev_helper.sh
```

**Option 2: Direct Commands**
```bash
# Build for ESP32
/home/brucequinton/.local/bin/pio run -e esp32doit-devkit-v1

# Upload to ESP32 (connect via USB first)
/home/brucequinton/.local/bin/pio run -e esp32doit-devkit-v1 --target upload

# Upload and monitor serial output
/home/brucequinton/.local/bin/pio run -e esp32doit-devkit-v1 --target upload --target monitor
```

**Option 3: VS Code Tasks**
1. Open in VS Code: `code .`
2. Press `Ctrl+Shift+P`
3. Type: `Tasks: Run Task`
4. Select: `PlatformIO: Build` or `PlatformIO: Upload`

3. **Open Project**
   - File → Open Folder
   - Select the `hunter_wifi_remote` folder
   - PlatformIO will detect the `platformio.ini` file automatically

### Option 2: PlatformIO Core (Command Line)

1. **Install PlatformIO Core**
   ```bash
   # On Linux/Mac with Python
   pip install platformio

   # On Windows with Python
   pip install platformio

   # Or install via script
   curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
   python3 get-platformio.py
   ```

2. **Navigate to project**
   ```bash
   cd /path/to/hunter_wifi_remote
   ```

## Project Structure

```
hunter_wifi_remote/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp            # Main application code
│   └── hunter_esp32.cpp    # Hunter protocol implementation
├── include/
│   └── hunter_esp32.h      # Hunter protocol header
├── README_ESP32.md         # Arduino IDE setup guide
└── README_PLATFORMIO.md    # This file
```

## Configuration

### 1. Board Selection

The `platformio.ini` is configured for ESP32 DevKit v1, but you can change it for other ESP32 boards:

```ini
[env:esp32doit-devkit-v1]    # Standard ESP32 DevKit
# [env:lolin_d32]            # WEMOS LOLIN D32
# [env:esp32-s3-devkitc-1]   # ESP32-S3 DevKit
# [env:esp32-c3-devkitc-02]  # ESP32-C3 DevKit
```

### 2. WiFi Credentials

Update WiFi settings in `src/main.cpp`:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

**Alternative: Environment Variables**
You can set WiFi credentials as build flags in `platformio.ini`:
```ini
build_flags =
    -DWIFI_SSID=\"YourWiFiSSID\"
    -DWIFI_PASSWORD=\"YourWiFiPassword\"
```

Then in `main.cpp`:
```cpp
#ifndef WIFI_SSID
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
#else
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
#endif
```

### 3. Pin Configuration

Modify pin assignments in `include/hunter_esp32.h`:
```cpp
#define HUNTER_PIN 15    // Communication pin
```

And in `src/main.cpp`:
```cpp
const int PUMP_PIN = 5;  // Pump control pin
```

## Building and Uploading

### VS Code Method

1. **Open PlatformIO**
   - Click the PlatformIO icon in the left sidebar
   - Or press Ctrl+Shift+P and type "PlatformIO"

2. **Build Project**
   - Click "Build" in PlatformIO toolbar
   - Or use: Ctrl+Alt+B

3. **Upload to ESP32**
   - Connect ESP32 via USB
   - Click "Upload" in PlatformIO toolbar
   - Or use: Ctrl+Alt+U

4. **Monitor Serial Output**
   - Click "Serial Monitor" in PlatformIO toolbar
   - Or use: Ctrl+Alt+S

### Command Line Method

```bash
# Build the project
pio run

# Upload to connected ESP32
pio run --target upload

# Open serial monitor
pio device monitor

# Build, upload, and monitor in one command
pio run --target upload --target monitor
```

## Advanced Features

### Over-the-Air Updates (OTA)

Once your ESP32 is running, you can update it wirelessly:

1. **Enable OTA in platformio.ini**
   ```ini
   upload_protocol = espota
   upload_port = 192.168.1.100  ; Replace with your ESP32's IP
   ```

2. **Upload via OTA**
   ```bash
   pio run --target upload
   ```

### Environment-Specific Builds

You can create multiple environments for different configurations:

```ini
[env:development]
platform = espressif32
board = esp32doit-devkit-v1
build_flags = -DDEBUG_MODE=1

[env:production]
platform = espressif32
board = esp32doit-devkit-v1
build_flags = -DPRODUCTION_MODE=1
```

Build specific environment:
```bash
pio run -e development
pio run -e production
```

### Unit Testing

Create tests in `test/` directory:

```cpp
// test/test_hunter.cpp
#include <unity.h>
#include "hunter_esp32.h"

void test_zone_validation() {
    // Test zone number validation
    TEST_ASSERT_TRUE(zone_valid(1));
    TEST_ASSERT_TRUE(zone_valid(48));
    TEST_ASSERT_FALSE(zone_valid(0));
    TEST_ASSERT_FALSE(zone_valid(49));
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_zone_validation);
    UNITY_END();
}

void loop() {}
```

Run tests:
```bash
pio test
```

### Debugging

For hardware debugging with supported debuggers:

```ini
debug_tool = esp-prog
debug_init_break = tbreak setup
```

## Troubleshooting

### Common Issues

1. **ESP32 not detected**
   ```bash
   # List connected devices
   pio device list

   # Check if driver is installed
   # Windows: Install CP210x or CH340 driver
   # Linux: Add user to dialout group
   sudo usermod -a -G dialout $USER
   ```

2. **Build errors**
   ```bash
   # Clean build cache
   pio run --target clean

   # Update platform
   pio platform update espressif32
   ```

3. **Library issues**
   ```bash
   # Update libraries
   pio lib update

   # Install specific library version
   pio lib install "me-no-dev/ESP Async WebServer@1.2.3"
   ```

4. **Upload fails**
   - Hold BOOT button on ESP32 during upload
   - Check cable is data-capable (not charge-only)
   - Try different USB port
   - Reduce upload speed in `platformio.ini`:
     ```ini
     upload_speed = 460800
     ```

### Serial Monitor Issues

If you can't see serial output:
- Check baud rate matches (115200)
- Try different monitor filters:
  ```ini
  monitor_filters =
      default
      time
      esp32_exception_decoder
  ```

## VS Code Tips

### Useful Extensions

- **PlatformIO IDE** - Main PlatformIO support
- **C/C++** - Microsoft C++ extension
- **GitLens** - Git integration
- **Bracket Pair Colorizer** - Visual bracket matching

### Keyboard Shortcuts

- **Ctrl+Alt+B** - Build project
- **Ctrl+Alt+U** - Upload firmware
- **Ctrl+Alt+S** - Serial monitor
- **Ctrl+Shift+P** - Command palette
- **F12** - Go to definition
- **Ctrl+Space** - Trigger intellisense

### IntelliSense Configuration

PlatformIO automatically configures IntelliSense, but you can customize it in `.vscode/c_cpp_properties.json`:

```json
{
    "configurations": [
        {
            "name": "PlatformIO",
            "includePath": [
                "${workspaceFolder}/include",
                "${workspaceFolder}/src",
                "${workspaceFolder}/.pio/libdeps/*/src"
            ],
            "defines": [
                "ARDUINO=10813",
                "ESP32"
            ],
            "compilerPath": "${workspaceFolder}/.pio/build/esp32doit-devkit-v1/idedata.json"
        }
    ],
    "version": 4
}
```

## Performance Optimization

### Build Optimization

```ini
# Optimize for size
build_flags = -Os

# Optimize for speed
build_flags = -O2

# Enable all optimizations
build_flags = -O3 -DCORE_DEBUG_LEVEL=0
```

### Memory Management

Monitor memory usage:
```cpp
void setup() {
    Serial.begin(115200);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
}
```

## Deployment

### Production Build

```ini
[env:production]
platform = espressif32
board = esp32doit-devkit-v1
build_flags =
    -DPRODUCTION_MODE=1
    -DCORE_DEBUG_LEVEL=0
    -Os
lib_deps =
    me-no-dev/ESP Async WebServer@^1.2.3
    me-no-dev/AsyncTCP@^1.1.1
```

### Firmware Distribution

Generate firmware files for distribution:
```bash
# Build firmware
pio run

# Firmware files are in:
# .pio/build/esp32doit-devkit-v1/firmware.bin
# .pio/build/esp32doit-devkit-v1/partitions.bin
```

Users can flash using:
```bash
# Using esptool.py
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x1000 firmware.bin

# Using PlatformIO
pio run --target upload --upload-port /dev/ttyUSB0
```

## Getting Help

- **PlatformIO Documentation**: https://docs.platformio.org/
- **ESP32 Arduino Documentation**: https://docs.espressif.com/projects/arduino-esp32/
- **Community Forum**: https://community.platformio.org/
- **GitHub Issues**: https://github.com/platformio/platformio-core/issues

## Next Steps

Once you have the basic setup working:

1. **Customize the web interface** in `src/main.cpp`
2. **Add new features** like scheduling or weather integration
3. **Implement OTA updates** for remote firmware updates
4. **Add unit tests** for critical functions
5. **Set up CI/CD** for automated building and testing

Happy coding with PlatformIO! 🚀