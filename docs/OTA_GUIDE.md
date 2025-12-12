# ESP32 OTA (Over-The-Air) Firmware Update Guide

## Overview
The ESP32 Irrigation Controller now supports **Over-The-Air (OTA)** firmware updates, allowing you to flash new firmware remotely over WiFi without needing USB access.

## Features
- ✅ **Remote Updates**: Flash firmware over WiFi from anywhere on your network
- ✅ **Safe Updates**: Automatically stops all irrigation during OTA to prevent issues
- ✅ **Auto-Resume**: Scheduler automatically resumes after failed OTA attempts
- ✅ **Hostname Based**: Device advertises itself as `irrigation-<DEVICE_ID>`
- ✅ **Dual Mode**: Keep USB upload for emergencies, use OTA for convenience

## Initial Setup

### 1. First Upload via USB (One-Time)
You **must** perform the initial firmware upload via USB to enable OTA:

```bash
cd /home/brucequinton/Projects/home-irrigation-system/esp32-irrigation-controller

# Build and upload via USB
pio run --target upload

# Monitor serial output to get ESP32's IP address
pio device monitor
```

**Note the IP address** displayed in the serial monitor (e.g., `192.168.1.100`)

### 2. Update platformio.ini (Optional)
Edit `platformio.ini` and change the default OTA upload port to your ESP32's IP:

```ini
[env:esp32-ota]
upload_port = 192.168.1.100  ; Change to YOUR ESP32's IP
```

## Using OTA Updates

### Method 1: Command Line (Recommended)

```bash
# Upload firmware to ESP32 via OTA
pio run -e esp32-ota --target upload --upload-port 192.168.1.100

# Replace 192.168.1.100 with your ESP32's actual IP address
```

### Method 2: Using Configured IP (if you edited platformio.ini)

```bash
# If you updated the upload_port in platformio.ini
pio run -e esp32-ota --target upload
```

### Method 3: VS Code PlatformIO Extension

1. Click **PlatformIO** icon in sidebar
2. Expand **PROJECT TASKS**
3. Expand **esp32-ota**
4. Click **Upload** (will use IP from `platformio.ini`)

Or:

1. Press `Ctrl+Shift+P`
2. Type: `PlatformIO: Upload`
3. Select **esp32-ota** environment
4. Modify upload command to include `--upload-port <IP>`

## OTA Process

### What Happens During OTA:

1. **Pre-Update**:
   - All irrigation zones are immediately stopped
   - Schedule manager is paused
   - OTA update begins

2. **Update Progress**:
   - Progress shown in terminal (0-100%)
   - ESP32 receives new firmware over WiFi
   - Flash memory is updated

3. **Completion**:
   - **Success**: ESP32 automatically reboots with new firmware
   - **Failure**: Scheduler resumes automatically, ESP32 continues with old firmware

### Serial Monitor Output Example:

```
OTA: Starting update (sketch)
OTA Progress: 0%
OTA Progress: 25%
OTA Progress: 50%
OTA Progress: 75%
OTA Progress: 100%
OTA: Update complete!

[ESP32 Reboots]

WiFi connected successfully!
IP address: 192.168.1.100
OTA initialized successfully
  Hostname: irrigation-device-01
  IP Address: 192.168.1.100
```

## Troubleshooting

### OTA Upload Fails

1. **Check ESP32 is online**:
   ```bash
   ping 192.168.1.100
   ```

2. **Verify OTA is initialized**:
   - Check serial monitor for: "OTA initialized successfully"
   - Confirm hostname: `irrigation-<device-id>`

3. **Check firewall**:
   - OTA uses port 3232
   - Ensure your firewall allows UDP/TCP on port 3232

4. **Try USB upload as fallback**:
   ```bash
   pio run --target upload  # Uses USB automatically
   ```

### ESP32 Won't Boot After OTA

If OTA update corrupts firmware:

1. **Connect via USB**
2. **Re-flash via USB**:
   ```bash
   pio run --target upload
   ```

## Security Considerations

### Add OTA Password (Recommended)

1. **Edit `platformio.ini`**:
   ```ini
   [env:esp32-ota]
   upload_flags =
       --port=3232
       --auth=your-secure-password  ; Add password here
   ```

2. **Edit `src/main.cpp`**:
   ```cpp
   // Uncomment and set password
   ArduinoOTA.setPassword("your-secure-password");
   ```

3. **Rebuild and upload**

### Network Isolation

- Consider placing ESP32 on isolated VLAN
- Use firewall rules to restrict OTA access to trusted IPs

## Quick Reference Commands

```bash
# Initial USB flash (required once)
pio run --target upload

# OTA update (specify IP each time)
pio run -e esp32-ota --target upload --upload-port 192.168.1.100

# Monitor serial output
pio device monitor

# Build only (no upload)
pio run

# Clean build
pio run --target clean

# List available devices
pio device list
```

## Workflow

### Daily Development (Remote Updates)
```bash
# 1. Edit code locally
vim src/main.cpp

# 2. Upload via OTA
pio run -e esp32-ota --target upload --upload-port 192.168.1.100

# 3. Monitor logs remotely (if needed)
ssh bruce.quinton@172.17.254.10 "tail -f /var/log/irrigation.log"
```

### Emergency Recovery (USB Required)
```bash
# Connect ESP32 via USB cable
pio run --target upload
```

## Benefits

- ✅ **No Physical Access**: Update firmware without accessing ESP32
- ✅ **Faster Development**: No need to connect/disconnect USB
- ✅ **Remote Deployment**: Update production devices from anywhere
- ✅ **Safe Fallback**: USB upload always available if OTA fails
- ✅ **Zero Downtime**: Quick updates with minimal irrigation disruption

## Memory Impact

OTA adds approximately:
- **Flash**: ~10KB (ArduinoOTA library)
- **RAM**: ~2KB during operation
- **Total**: Minimal impact on ESP32's 4MB flash and 520KB RAM

Current usage with OTA:
- Flash: ~860KB / 4MB (21%)
- RAM: ~48KB / 520KB (9%)

## Next Steps

1. Flash initial firmware via USB
2. Note ESP32's IP address
3. Test OTA upload: `pio run -e esp32-ota --target upload --upload-port <IP>`
4. Consider adding OTA password for security
5. Document ESP32 IP address for future OTA updates

---

**Happy remote flashing!** 🚀
