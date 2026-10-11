# 🏎️ ESP32 CYD Auto Dashboard (Bluetooth OBD-II)

[![PlatformIO CI](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg?logo=platformio)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue.svg?logo=arduino)](https://www.arduino.cc/)
[![Target Hardware](https://img.shields.io/badge/Hardware-ESP32--CYD-yellow.svg)](https://github.com/9M2PJU/9M2PJU-ESP32-CYD-Auto-Dashboard)
[![OBD-II Protocol](https://img.shields.io/badge/OBD--II-ELM327%20BT%20Classic-green.svg)](https://en.wikipedia.org/wiki/OBD-II_PIDs)
[![License: PolyForm Noncommercial](https://img.shields.io/badge/License-PolyForm_Noncommercial_1.0.0-blue.svg)](LICENSE)

An advanced automotive digital gauge cluster and live telemetry dashboard firmware for the **ESP32 Cheap Yellow Display (CYD)** family (ESP32-2432S028, ESP32-2432S024, ESP32-2432S032, and ESP32-3248S035).

It connects wirelessly via **Bluetooth Classic (SPP)** to any standard **ELM327 OBD-II adapter** to read and visualize real-time powertrain metrics with high-refresh vector graphics, dynamic shift lights, drag acceleration timers, and diagnostic fault code scanning.

---

## 📸 Dashboard Screens & Architecture

```
+-------------------------------------------------------------------+
|  [●] OBD CONNECTED          9M2PJU Auto                    14.2V  | <- Top Status Bar
+-------------------------------------------------------------------+
|                                                                   |
|   +----------+          /-----------\          +----------+       |
|   | COOLANT  |         /     RPM     \         | THROTTLE |       |
|   |  89 °C   |        |     6240      |        |   78 %   |       |
|   | [||||||] |        |               |        | [||||||] |       |
|   |   IAT    |        |     128       |        |   LOAD   |       |
|   |  34 °C   |         \    KM/H     /         |   64 %   |       |
|   +----------+          \----[4]----/          +----------+       |
|                                                                   |
+-------------------------------------------------------------------+
|  [ SPORT ]   [ GRID ]   [ DRAG ]   [ DIAG ]   [ SETUP ]           | <- Bottom Navigation
+-------------------------------------------------------------------+
```

---

## 🌟 Core Features

### 1. 🏎️ Sport Cluster (Main Gauge View)
* **Dynamic RPM Tachometer Arc**: Sweeping vector arc with color zones (Green $\rightarrow$ Yellow $\rightarrow$ Redline) and live numeric RPM.
* **Large Digital Speedometer**: Central high-visibility speed readout in **KM/H** or **MPH** (tap to toggle).
* **Estimated Gear Indicator**: Real-time gear badge (*P, N, 1, 2, 3, 4, 5, 6*) calculated from vehicle speed vs. engine RPM.
* **Coolant & Intake Temperature (IAT)**: Left vertical bar gauge with cold, operating, and overheat alarm levels.
* **Throttle Position & Engine Load Dual Meters**: Right vertical twin bar graphs.
* **Top Status & Shift Banner**: Live Bluetooth state, coolant overheat alert, and battery voltage badge.

### 2. 📊 Telemetry Grid (6-Sensor Live Hub)
High-contrast digital gauge cards displaying 6 simultaneous live parameters:
1. **Coolant Temperature** (°C / °F) with status bar
2. **Intake Air Temperature (IAT)** (°C / °F)
3. **Battery / Control Module Voltage** (V) with charging indicator
4. **Calculated Engine Load** (%)
5. **Throttle Position (TPS)** (%)
6. **Manifold Absolute Pressure (MAP) / Turbo Boost** (kPa / PSI)

### 3. ⏱️ Performance & Drag Acceleration Timer
* **Automatic Launch Detection**: Detects vehicle launch from standstill (0 km/h) automatically.
* **Split Timers**:
  * **0 – 60 km/h** (0 – 37 mph)
  * **0 – 100 km/h** (0 – 62 mph / 0-60 mph equivalent)
  * **400m (1/4 Mile)** split time and trap speed
* **Real-time G-Force Meter**: Instantaneous acceleration force ($G$) calculation with peak hold.
* **Best Run Memory**: Automatically stores your fastest acceleration runs.
* **Re-Arm Button**: One-tap reset/arm for repeatable testing.

### 4. 🛠️ OBD-II Diagnostics & DTC Fault Code Scanner
* **Check Engine Light (MIL) Status**: Live MIL state indicator (Green = Normal, Red = Active Fault).
* **Mode 03 Trouble Code Reader**: Reads active stored diagnostic trouble codes from the ECU (e.g. `P0300`, `P0171`, `P0420`) with human-readable fault descriptions.
* **Mode 04 DTC Clear Tool**: Clears trouble codes and resets the MIL with a safety confirmation dialog.

### 5. 🎨 5 Selectable Dashboard Color Themes
Customize the gauge and dashboard aesthetics with 5 high-contrast color palettes selectable on the **SETUP** page:
* **Cyber Cyan** *(Default)*: Modern carbon black with electric cyan and ice blue accents.
* **Racing Red**: High-intensity Type-R sport theme with carbon charcoal and crimson red accents.
* **Nismo Amber**: Classic JDM / BMW cluster aesthetic with warm amber and orange illumination.
* **Neon Green**: Acid lime / RS sport theme with dark forest background and toxic green highlights.
* **Electric Blue**: GT cobalt blue aesthetic with deep ocean black and brilliant electric blue accents.

### 6. ⚙️ Settings & Customization
* **Built-in Driving Simulator (Demo Mode)**: Test all animations, gauges, revs, and gear shifts on your desk without an OBD dongle!
* **Color Theme Switcher**: Instant one-touch theme toggling across all gauge screens.
* **Unit System**: Switch between **Metric** (km/h, °C) and **Imperial** (mph, °F).
* **Shift Light RPM Adjuster**: Configure shift alert RPM from 3000 to 8500 RPM.
* **Backlight Auto-Dimming**: Automatic ambient light tracking using the onboard LDR or manual brightness.
* **Audio & Buzzer**: Enable/disable shift beeps, over-temperature alarms, and touch clicks.
* **Bluetooth Device Selector**: Quickly cycle through common adapter names (`OBDII`, `Viecar`, `vGate`, `OBDLink`).

---

## 💡 Hardware Shift Light & Peripheral Integration

The firmware utilizes all onboard hardware peripherals of the ESP32 CYD:

| Peripheral | ESP32 GPIO | Behavior / Function |
|---|---|---|
| **RGB LED (Red)** | `GPIO 4` (Active LOW) | Progressive shift light (Stage 3), Redline strobe, Overheat flash |
| **RGB LED (Green)** | `GPIO 16` (Active LOW) | Progressive shift light (Stage 1 & 2), Connected status |
| **RGB LED (Blue)** | `GPIO 17` (Active LOW) | Bluetooth connection pairing pulse, Night ambient glow |
| **Ambient Light Sensor (LDR)** | `GPIO 34` (ADC1) | Auto-brightness dimming for night driving |
| **Audio Buzzer / DAC** | `GPIO 26` (PWM) | Shift beep, overheat/low-voltage alarm, touch feedback |
| **TFT Backlight** | `GPIO 21` / `GPIO 27` | Hardware PWM dimming (15% to 100%) |
| **Touch Screen** | `GPIO 33, 25, 32, 39, 36` | Full gesture support (Swipe Left/Right for pages, tap buttons) |

---

## 📟 Supported CYD Hardware Variants

This codebase supports all major ESP32 CYD board models via dedicated PlatformIO environments:

| Environment Target | Board Model | Screen Size | Resolution | Display Controller | Touch Controller |
|---|---|---|---|---|---|
| `esp32-cyd-28r` *(Default)* | ESP32-2432S028R | 2.8" | 320 $\times$ 240 | ILI9341 (HSPI) | XPT2046 (VSPI) |
| `esp32-cyd-28-2usb` | ESP32-2432S028 (2-USB) | 2.8" | 320 $\times$ 240 | ST7789 (HSPI) | XPT2046 (VSPI) |
| `esp32-cyd-24r` | ESP32-2432S024R | 2.4" | 320 $\times$ 240 | ILI9341 (HSPI) | XPT2046 (VSPI) |
| `esp32-cyd-32r` | ESP32-2432S032R | 3.2" | 320 $\times$ 240 | ST7789 (HSPI) | XPT2046 (VSPI) |
| `esp32-cyd-35r` | ESP32-3248S035R | 3.5" | 480 $\times$ 320 | ST7796 (HSPI) | XPT2046 (VSPI) |

---

## 🔌 Bluetooth OBD-II Pairing Guide

1. Plug your **Bluetooth ELM327 OBD-II adapter** into your vehicle's OBD-II port (usually under the steering wheel / dashboard).
2. Turn the vehicle ignition to **ON** (or start the engine).
3. Power on the **ESP32 CYD**.
4. The CYD will automatically scan and pair with any adapter named `OBDII`, `Viecar`, `vGate`, or `OBDLink`.
5. Once connected, the top status dot turns **Green** and live telemetry begins streaming immediately.
6. If your adapter has a custom name, navigate to the **SETUP** page and tap the **OBD BT** button to select or edit your device name.

---

## 🚀 Building & Flashing

### Requirements
* [PlatformIO](https://platformio.org/) installed (CLI or VS Code extension).
* USB Cable (Micro-USB or Type-C depending on your CYD board).

### 1. Clone the Repository
```bash
git clone https://github.com/9M2PJU/9M2PJU-ESP32-CYD-Auto-Dashboard.git
cd 9M2PJU-ESP32-CYD-Auto-Dashboard
```

### 2. Build for Your Board Model
```bash
# Build for standard 2.8" CYD (Default)
pio run -e esp32-cyd-28r

# Or build for 2.8" 2-USB variant
pio run -e esp32-cyd-28-2usb

# Or build for 2.4" CYD
pio run -e esp32-cyd-24r

# Or build for 3.2" CYD
pio run -e esp32-cyd-32r

# Or build for 3.5" CYD (ST7796)
pio run -e esp32-cyd-35r
```

### 3. Flash to Board
Connect your CYD to USB and run:
```bash
pio run -e esp32-cyd-28r --target upload
```

### 4. Serial Monitor
```bash
pio run -e esp32-cyd-28r --target monitor
```

---

## 👆 Touch Navigation & Gestures

* **Swipe Left**: Switch to Next Page (`SPORT` $\rightarrow$ `GRID` $\rightarrow$ `DRAG` $\rightarrow$ `DIAG` $\rightarrow$ `SETUP`).
* **Swipe Right**: Switch to Previous Page.
* **Bottom Tabs**: Tap any tab directly to jump to that page.
* **Speedometer Tap**: Tap the center speedometer on the Sport Cluster to toggle between **KM/H** and **MPH**.
* **Grid Card Tap**: Tap any card on the Grid page to toggle between **Metric** and **Imperial** units.
* **Drag Timer Button**: Tap `RESET / RE-ARM TIMER` to arm for a new run.
* **DTC Action Buttons**: Tap `SCAN DTCs` to query fault codes, or `CLEAR CODES` to reset the MIL.

---

## 📄 License

This project is licensed under the **PolyForm Noncommercial License 1.0.0** — see the [LICENSE](LICENSE) file for details.

* **Personal & Hobbyist Use**: Free for personal, amateur, research, and non-commercial vehicle builds.
* **Commercial Rights**: Commercial distribution, pre-flashed device sales, and monetization are reserved exclusively by **[9M2PJU](https://github.com/9M2PJU)**.

Developed with ❤️ by **[9M2PJU](https://github.com/9M2PJU)**.

## Support

If you find this project useful, please consider supporting it:

[![Donate with PayPal](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=9m2pju%40hamradio.my&currency_code=MYR&item_name=9M2PJU-ESP32-CYD-Auto-Dashboard)
