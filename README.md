# 9M2PJU-ESP32-CYD-Auto-Dashboard

An advanced automotive digital gauge cluster and live telemetry dashboard firmware for the **ESP32 Cheap Yellow Display (CYD / ESP32-2432S028R)**. It connects wirelessly via **Bluetooth Classic (SPP)** to any standard **ELM327 OBD-II adapter** to read and visualize real-time vehicle parameters with high-refresh graphics, customizable shift lights, acceleration timers, and diagnostic fault code scanning.

---

## 🏎️ Features & Dashboard Views

### 1. Sport Cluster Dashboard (Flagship View)
* **Sweeping RPM Tachometer Arc**: Dynamic color-coded arc (Green $\rightarrow$ Yellow $\rightarrow$ Redline) with real-time numeric RPM readout.
* **Large Digital Speedometer**: Center high-contrast speed readout in **KM/H** or **MPH**.
* **Estimated Gear Indicator**: Dynamic gear badge (P, N, 1, 2, 3, 4, 5, 6) calculated from RPM/Speed ratio.
* **Engine Coolant Temperature Bar**: Left vertical gauge with cold/normal/hot warning levels and IAT readout.
* **Throttle Position & Engine Load Dual Meters**: Right vertical twin bar graphs.
* **Top Shift Light Bar & Status Banner**: Live connectivity indicator and shift RPM warnings.

### 2. Telemetry Multi-Sensor Grid (6-Card Hub)
* **6 Live Sensor Cards**:
  1. **Coolant Temp** (°C / °F) with status bar
  2. **Intake Air Temp (IAT)** (°C / °F)
  3. **Battery / Module Voltage** (V) with charging indicator
  4. **Calculated Engine Load** (%)
  5. **Throttle Position (TPS)** (%)
  6. **Manifold Absolute Pressure (MAP) / Turbo Boost** (kPa / PSI)

### 3. Drag & Acceleration Performance Timer
* **Automatic Launch Detection**: Detects vehicle launch from standstill.
* **Split Timers**:
  * **0 - 60 km/h** (0 - 37 mph)
  * **0 - 100 km/h** (0 - 62 mph / 0-60 mph equivalent)
  * **400m (1/4 Mile)** time and trap speed
* **Peak G-Force Meter**: Real-time calculated acceleration force ($G$).
* **Best Record Memory**: Stores your fastest 0-100 km/h run.
* **Re-arm / Reset Button**: Manual or automatic trigger.

### 4. OBD-II Diagnostics & DTC Fault Code Scanner
* **Check Engine Light (MIL) Status**: Real-time MIL alert indicator (Green = Normal, Red = Active Fault).
* **Mode 03 DTC Reader**: Reads stored diagnostic trouble codes (e.g. `P0300`, `P0171`, `P0420`) with human-readable descriptions.
* **Mode 04 DTC Clear Tool**: Clears fault codes from the ECU with a safety confirmation dialog.

### 5. Settings & Customization
* **Built-in Driving Simulator (Demo Mode)**: Test all animations, gauges, revs, and gear shifting directly on your desk without an OBD dongle!
* **Unit System Selection**: Toggle between **Metric** (km/h, °C) and **Imperial** (mph, °F).
* **Shift Light RPM Adjuster**: Configure shift alert RPM from 3000 to 8500 RPM.
* **Display Brightness**: Automatic ambient light dimming via LDR or manual level.
* **Sound / Buzzer Toggle**: Enable/disable shift beeps, over-temp alarms, and touch clicks.
* **Bluetooth OBD Target Selector**: Cycle between common adapter names (`OBDII`, `Viecar`, `vGate`, `OBDLink`).

---

## 💡 Hardware Shift Light & Peripheral Integration

| Component | Pin | Function |
|---|---|---|
| **RGB LED (Red)** | `GPIO 4` | Progressive shift light, redline strobe, coolant overheat flash |
| **RGB LED (Green)** | `GPIO 16` | Shift light progressive stage, connection indicator |
| **RGB LED (Blue)** | `GPIO 17` | Bluetooth pairing pulse, status glow |
| **Ambient Light Sensor (LDR)** | `GPIO 34` | Auto-brightness dimming for night driving |
| **Audio Buzzer / Speaker** | `GPIO 26` | Shift point beep, overheat alarm, touch click |
| **TFT Backlight PWM** | `GPIO 21` | Hardware PWM backlight control |
| **Resistive Touch (XPT2046)** | `GPIO 33, 25, 32, 39, 36` | Full gesture and button interaction |

---

## 📌 CYD (ESP32-2432S028R) Pinout

```
+-------------------------------------------------------------+
| ESP32 CYD 2.8" (ESP32-2432S028R) Pin Mapping                |
+---------------------+-------------------+-------------------+
| Function            | ESP32 GPIO Pin    | Protocol / Type   |
+---------------------+-------------------+-------------------+
| TFT MOSI            | GPIO 13           | HSPI              |
| TFT MISO            | GPIO 12           | HSPI              |
| TFT SCK             | GPIO 14           | HSPI              |
| TFT CS              | GPIO 15           | Output            |
| TFT DC (RS)         | GPIO 2            | Output            |
| TFT Backlight (BL)  | GPIO 21           | PWM (LEDC Ch 0)   |
| Touch CS            | GPIO 33           | VSPI              |
| Touch CLK           | GPIO 25           | VSPI              |
| Touch MOSI (DIN)    | GPIO 32           | VSPI              |
| Touch MISO (DOUT)   | GPIO 39           | VSPI (Input Only) |
| Touch IRQ           | GPIO 36           | Input Only        |
| RGB LED (Red)       | GPIO 4            | PWM (Active Low)  |
| RGB LED (Green)     | GPIO 16           | PWM (Active Low)  |
| RGB LED (Blue)      | GPIO 17           | PWM (Active Low)  |
| LDR Light Sensor    | GPIO 34           | ADC1 Analog Input |
| Buzzer / Audio      | GPIO 26           | PWM (LEDC Ch 4)   |
+---------------------+-------------------+-------------------+
```

---

## 🚀 Getting Started & Flashing

### 1. Requirements
* [PlatformIO IDE](https://platformio.org/) (VS Code extension or CLI)
* ESP32 CYD Board (ESP32-2432S028R)
* Bluetooth ELM327 OBD-II adapter (standard 12V automotive OBD port dongle)

### 2. Build & Upload via PlatformIO CLI
```bash
# Clone repository
git clone https://github.com/9M2PJU/9M2PJU-ESP32-CYD-Auto-Dashboard.git
cd 9M2PJU-ESP32-CYD-Auto-Dashboard

# Compile firmware
pio run

# Flash to CYD board (ensure USB cable is connected)
pio run --target upload

# Open serial monitor
pio run --target monitor
```

---

## 📱 Navigation & Gestures

* **Bottom Tabs**: Tap any tab (`SPORT`, `GRID`, `DRAG`, `DIAG`, `SETUP`) to switch views.
* **Swipe Gestures**: Swipe Left anywhere on the screen for the Next Page, Swipe Right for the Previous Page.
* **Speedometer Tap**: Tap the center speedometer on the Sport Cluster to instantly toggle between KM/H and MPH.
* **Grid Card Tap**: Tap any telemetry card on the Grid page to toggle between Metric and Imperial temperature units.

---

## 📄 License
This project is open-source under the MIT License. Developed by **9M2PJU**.
