#!/bin/bash

# Hunter WiFi Remote - Development Helper
echo "🌱 Hunter WiFi Remote - ESP32 Development Helper"
echo "================================================"

PIO_CMD="/home/brucequinton/.local/bin/pio"

# Check if PlatformIO is available
if [ ! -f "$PIO_CMD" ]; then
    echo "❌ PlatformIO not found at $PIO_CMD"
    echo "Please install PlatformIO Core first"
    exit 1
fi

# Check if ESP32 is connected (for upload operations)
check_esp32() {
    if ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1; then
        ESP32_PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1)
        echo "✅ ESP32 found at: $ESP32_PORT"
        return 0
    else
        echo "❌ No ESP32 detected. Please connect via USB."
        echo "   Make sure you're using a data cable (not charge-only)"
        return 1
    fi
}

# Menu
echo ""
echo "Available ESP32 Targets:"
echo "• esp32doit-devkit-v1 (ESP32 D1 Mini / ESP-WROOM-32)"
echo "• esp32-s2-saola-1 (ESP32-S2)"
echo "• esp32-s3-devkitc-1 (ESP32-S3)"
echo ""
echo "What would you like to do?"
echo "1. Build all targets"
echo "2. Build ESP32 only"
echo "3. Build ESP32-S2 only"
echo "4. Build ESP32-S3 only"
echo "5. Upload to ESP32"
echo "6. Upload to ESP32-S2"
echo "7. Upload to ESP32-S3"
echo "8. Upload and Monitor (ESP32)"
echo "9. Monitor Serial Output"
echo "10. Clean build"
echo "11. Project Status"
echo "12. Start VS Code"
echo ""

read -p "Enter choice (1-12): " choice

case $choice in
    1)
        echo "🔨 Building project for ESP32..."
        $PIO_CMD run -e esp32doit-devkit-v1
        ;;
case $choice in
    1)
        echo "🔨 Building all targets..."
        $PIO_CMD run
        ;;
    2)
        echo "🔨 Building ESP32..."
        $PIO_CMD run -e esp32doit-devkit-v1
        ;;
    3)
        echo "🔨 Building ESP32-S2..."
        $PIO_CMD run -e esp32-s2-saola-1
        ;;
    4)
        echo "🔨 Building ESP32-S3..."
        $PIO_CMD run -e esp32-s3-devkitc-1
        ;;
    5)
        echo "📤 Uploading to ESP32..."
        if check_esp32; then
            $PIO_CMD run -e esp32doit-devkit-v1 --target upload
        fi
        ;;
    6)
        echo "📤 Uploading to ESP32-S2..."
        if check_esp32; then
            $PIO_CMD run -e esp32-s2-saola-1 --target upload
        fi
        ;;
    7)
        echo "📤 Uploading to ESP32-S3..."
        if check_esp32; then
            $PIO_CMD run -e esp32-s3-devkitc-1 --target upload
        fi
        ;;
    8)
        echo "📤🖥️ Uploading to ESP32 and starting monitor..."
        if check_esp32; then
            $PIO_CMD run -e esp32doit-devkit-v1 --target upload --target monitor
        fi
        ;;
    9)
        echo "🖥️ Starting serial monitor (Ctrl+C to exit)..."
        if check_esp32; then
            $PIO_CMD device monitor
        fi
        ;;
    10)
        echo "🧹 Cleaning build files..."
        $PIO_CMD run --target clean
        ;;
    11)
        echo "ℹ️ Project Status:"
        echo "📁 Build environments:"
        $PIO_CMD project config
        echo ""
        echo "📊 Build directory sizes:"
        du -sh .pio/build/* 2>/dev/null || echo "No builds found"
        ;;
    12)
        echo "🚀 Starting VS Code..."
        code . &
        echo "VS Code started in background"
        ;;
    *)
        echo "❌ Invalid choice. Please run again."
        exit 1
        ;;
esac

echo ""
echo "✅ Operation completed!"
echo ""
echo "💡 Quick Tips:"
echo "• Use VS Code tasks: Ctrl+Shift+P → Tasks: Run Task"
echo "• Manual commands: $PIO_CMD run"
echo "• Monitor serial: $PIO_CMD device monitor"
echo "• WiFi configured: Q-Home network"
    3)
        echo "🔨 Building project for ESP32-S3..."
        $PIO_CMD run -e esp32-s3-devkitc-1
        ;;
    4)
        echo "�📤 Uploading to ESP32..."
        $PIO_CMD run -e esp32doit-devkit-v1 --target upload
        ;;
    5)
        echo "📤 Uploading to ESP32-S2..."
        $PIO_CMD run -e esp32-s2-saola-1 --target upload
        ;;
    6)
        echo "📤 Uploading to ESP32-S3..."
        $PIO_CMD run -e esp32-s3-devkitc-1 --target upload
        ;;
    7)
        echo "��🖥️ Uploading and starting monitor (ESP32)..."
        echo "Press Ctrl+C to exit monitor"
        $PIO_CMD run -e esp32doit-devkit-v1 --target upload --target monitor
        ;;
    8)
        echo "📤🖥️ Uploading and starting monitor (ESP32-S2)..."
        echo "Press Ctrl+C to exit monitor"
        $PIO_CMD run -e esp32-s2-saola-1 --target upload --target monitor
        ;;
    9)
        echo "📤🖥️ Uploading and starting monitor (ESP32-S3)..."
        echo "Press Ctrl+C to exit monitor"
        $PIO_CMD run -e esp32-s3-devkitc-1 --target upload --target monitor
        ;;
    10)
        echo "🖥️ Starting serial monitor..."
        echo "Press Ctrl+C to exit monitor"
        $PIO_CMD device monitor
        ;;
    11)
        echo "🧹 Cleaning build files..."
        $PIO_CMD run --target clean
        ;;
    12)
        echo "📊 Project Status:"
        echo "   PlatformIO: $($PIO_CMD --version)"
        echo "   Project: $(pwd)"
        echo "   ESP32 Port: $ESP32_PORT"
        echo "   WiFi SSID: Q-Home (configured)"
        echo "   Supported boards: ESP32, ESP32-S2, ESP32-S3"
        echo ""
        echo "📁 Project Files:"
        ls -la src/ include/ platformio.ini 2>/dev/null
        ;;
    13)
        echo "🚀 Starting VS Code..."
        export PATH="$PATH:/home/brucequinton/.local/bin"
        code . &
        echo "VS Code started"
        ;;
    *)
        echo "❌ Invalid choice"
        exit 1
        ;;
esac

echo ""
echo "✅ Done!"
echo ""
echo "💡 Tips:"
echo "• After upload, check serial monitor for ESP32 IP address"
echo "• Access web interface at http://[ESP32_IP]"
echo "• WiFi credentials: Q-Home / MyD0nkey"
echo "• GPIO 16 → Hunter REM pin, 3.3V → Hunter AC pin"