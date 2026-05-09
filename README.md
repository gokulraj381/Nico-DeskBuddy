# Nico DeskBuddy

A cute desk companion powered by ESP32-C3 with an OLED face, speaker, touch sensor, and web interface.

## Features

- **Animated Eyes** — physics-based eyes with blinking, saccades, and mood expressions
- **Clock** — NTP-synced time display with blinking colon and date strip
- **Alarm** — set via web interface, rings with I2S speaker and animated flashing bell with screen shake
- **Reminders** — up to 5 reminders with animated bell notification screen
- **Dino Run Game** — touch-activated Chrome dino-style game with parallax clouds, dust particles, and night mode
- **Work Mode** — 15-minute focus timer with blinking colon countdown, then break screen with speaker chime
- **Web Dashboard** — mobile-friendly control panel for alarm, reminders, and work mode

## Default Display Cycle

Eyes (5s) → Clock (5s) → Alarm Info (5s) → Reminders List (5s) → repeat

Touch the sensor anytime to start the Dino game.

## How To Use

### Alarm
1. Open the web dashboard (IP shown on OLED after boot)
2. Under **Alarm** section, pick a time and tap **Set Alarm**
3. When the time hits, the speaker rings for 15 seconds with a flashing bell animation
4. Tap **Turn Off** on the web page to disable it

### Reminders
1. On the web dashboard, under **Reminders** section
2. Enter a time + message (max 30 chars), tap **Add Reminder**
3. Up to 5 reminders can be set
4. When a reminder triggers, an animated bell with the message shows for 5 seconds
5. Tap the **X** button next to any reminder to delete it

### Work Mode
1. Tap **Start 15 min Focus** on the web dashboard
2. OLED shows a big MM:SS countdown with blinking colon and progress bar
3. After 15 minutes, a break screen flashes with a chime from the speaker (5 seconds)
4. Touch the sensor to dismiss the break early
5. Tap **Stop Work Mode** on the web page to cancel anytime

### Dino Game
1. Touch the sensor anytime during the default display cycle to start
2. Touch to make the dino jump over cacti
3. Score increases as you clear obstacles, speed increases every 5 points
4. Night mode activates after score 15 (twinkling stars)
5. On game over, touch to exit back to normal display

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32-C3 Mini |
| Display | 1.3" SH1106G OLED 128x64 (I2C, 0x3C) |
| Speaker | MAX98357A I2S DAC |
| Touch | TTP223 capacitive touch sensor |

### Pin Mapping

| ESP32-C3 | Connected To |
|----------|-------------|
| GPIO8 | OLED SDA |
| GPIO9 | OLED SCL |
| GPIO4 | MAX98357A BCLK |
| GPIO3 | MAX98357A LRC (WS) |
| GPIO5 | MAX98357A DIN |
| GPIO0 | TTP223 OUT |

## Build & Upload

Built with [PlatformIO](https://platformio.org/).

```bash
pio run -e esp32c3 -t upload
```

## Configuration

Edit WiFi credentials in `src/main.cpp`:
```cpp
const char* wifiSsid = "YourSSID";
const char* wifiPass = "YourPassword";
```

After boot, the device shows its IP address on the OLED. Open it in a browser to access the web dashboard.

## Project Structure

```
├── src/
│   └── main.cpp          # All firmware code
├── platformio.ini         # PlatformIO build configuration
├── README.md
└── LICENSE
```

## Voice Assistant (INMP441)

This project was originally planned with voice assistant support using an **INMP441 I2S MEMS microphone**. However, the ESP32-C3 Mini has only **one I2S peripheral**, which is already used by the MAX98357A speaker for alarm tones and break chimes. Since both the speaker and mic require their own I2S bus, they cannot run simultaneously on the C3.

### v3 Plans

In **DeskBuddy v3**, the processor will be upgraded (e.g. ESP32-S3 or full ESP32) which has **two I2S peripherals** — one for the speaker and one for the INMP441 mic. This will enable:

- Wake-word detection
- Voice-controlled alarms and reminders
- Hands-free work mode start/stop

## License

MIT
