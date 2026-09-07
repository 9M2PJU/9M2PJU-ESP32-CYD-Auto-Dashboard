#pragma once

#include <Arduino.h>

// ============================================================================
// ESP32 CYD (Cheap Yellow Display - ESP32-2432S028R) Pin Definitions
// ============================================================================

// Display TFT Pins (HSPI) - Handled primarily via platformio.ini build flags
#define CYD_TFT_MOSI      13
#define CYD_TFT_MISO      12
#define CYD_TFT_SCK       14
#define CYD_TFT_CS        15
#define CYD_TFT_DC        2
#define CYD_TFT_RST       -1
#define CYD_TFT_BL        21  // Backlight PWM

// Touch Controller Pins (XPT2046 on separate SPI)
#define CYD_TOUCH_MOSI    32
#define CYD_TOUCH_MISO    39
#define CYD_TOUCH_CLK     25
#define CYD_TOUCH_CS      33
#define CYD_TOUCH_IRQ     36

// Onboard Peripherals
#define CYD_LED_RED       4   // Active LOW
#define CYD_LED_GREEN     16  // Active LOW
#define CYD_LED_BLUE      17  // Active LOW

#define CYD_LDR_PIN       34  // Photoresistor (Analog ADC1)
#define CYD_BUZZER_PIN    26  // Onboard speaker / DAC / Buzzer
#define CYD_SD_CS         5   // MicroSD CS (if needed)

// Screen Dimensions
#define SCREEN_WIDTH      320
#define SCREEN_HEIGHT     240

// Touch Calibration Defaults (Adjustable via Touch Calibration)
#define TOUCH_MIN_X       300
#define TOUCH_MAX_X       3800
#define TOUCH_MIN_Y       300
#define TOUCH_MAX_Y       3800

// ============================================================================
// Automotive Defaults & Thresholds
// ============================================================================
#define DEFAULT_OBD_NAME        "OBDII"
#define DEFAULT_SHIFT_RPM       6000
#define DEFAULT_REDLINE_RPM     7000
#define DEFAULT_MAX_RPM         8000
#define DEFAULT_COOLANT_WARN_C  105.0f
#define DEFAULT_VOLT_LOW_WARN   11.8f
#define DEFAULT_VOLT_HIGH_WARN  15.2f

// Application State / Settings Structure
struct AppSettings {
    char obdTargetName[32] = "OBDII";
    bool useImperial = false;       // false = km/h, °C; true = mph, °F
    bool demoMode = false;          // Built-in simulator
    bool soundEnabled = true;       // Beep on warnings/shifts/touch
    bool autoBrightness = true;     // Auto brightness using LDR
    uint8_t manualBrightness = 85;  // 10 - 100%
    uint16_t shiftRpm = 6000;       // Shift light trigger RPM
    uint16_t redlineRpm = 7000;     // Redline threshold
    bool autoCyclePages = false;    // Automatically cycle pages
    uint8_t cycleSeconds = 5;       // Auto cycle time
    uint16_t touchMinX = TOUCH_MIN_X;
    uint16_t touchMaxX = TOUCH_MAX_X;
    uint16_t touchMinY = TOUCH_MIN_Y;
    uint16_t touchMaxY = TOUCH_MAX_Y;
};

// UI Page Indices
enum DashboardPage {
    PAGE_SPORT_CLUSTER = 0,
    PAGE_TELEMETRY_GRID = 1,
    PAGE_PERFORMANCE = 2,
    PAGE_DIAGNOSTICS = 3,
    PAGE_SETTINGS = 4,
    PAGE_COUNT = 5
};
