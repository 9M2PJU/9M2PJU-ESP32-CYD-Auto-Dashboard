#include "hardware_manager.h"

#define LEDC_BL_CH       0
#define LEDC_RED_CH      1
#define LEDC_GRN_CH      2
#define LEDC_BLU_CH      3
#define LEDC_BUZ_CH      4

HardwareManager Hardware;

HardwareManager::HardwareManager()
    : m_touchSpi(VSPI),
      m_touch(CYD_TOUCH_CS, CYD_TOUCH_IRQ) {
}

void HardwareManager::begin() {
    // 1. Preferences & Settings
    m_prefs.begin("cyd_auto", false);
    if (m_prefs.isKey("cfg_init")) {
        m_prefs.getString("obd_name", m_settings.obdTargetName, sizeof(m_settings.obdTargetName));
        m_settings.useImperial = m_prefs.getBool("imperial", false);
        m_settings.demoMode = m_prefs.getBool("demo", false);
        m_settings.soundEnabled = m_prefs.getBool("sound", true);
        m_settings.autoBrightness = m_prefs.getBool("auto_br", true);
        m_settings.manualBrightness = m_prefs.getUChar("man_br", 85);
        m_settings.shiftRpm = m_prefs.getUShort("shift_rpm", DEFAULT_SHIFT_RPM);
        m_settings.redlineRpm = m_prefs.getUShort("redline_rpm", DEFAULT_REDLINE_RPM);
        m_settings.colorTheme = m_prefs.getUChar("theme", THEME_CYBER_CYAN);
        m_settings.autoCyclePages = m_prefs.getBool("cycle", false);
        m_settings.cycleSeconds = m_prefs.getUChar("cycle_sec", 5);
        m_settings.touchMinX = m_prefs.getUShort("t_min_x", TOUCH_MIN_X);
        m_settings.touchMaxX = m_prefs.getUShort("t_max_x", TOUCH_MAX_X);
        m_settings.touchMinY = m_prefs.getUShort("t_min_y", TOUCH_MIN_Y);
        m_settings.touchMaxY = m_prefs.getUShort("t_max_y", TOUCH_MAX_Y);
    } else {
        saveSettings();
    }

    // 2. Hardware PWM Setup
    initPwm();

    // 3. Touch Controller Init
    m_touchSpi.begin(CYD_TOUCH_CLK, CYD_TOUCH_MISO, CYD_TOUCH_MOSI, CYD_TOUCH_CS);
    m_touch.begin(m_touchSpi);
    m_touch.setRotation(1);

    // 4. LDR Pin setup
    pinMode(CYD_LDR_PIN, INPUT);

    // Initial Backlight & LED state
    setBacklight(m_settings.manualBrightness);
    setLedColor(0, 0, 0);

    // Startup beep
    if (m_settings.soundEnabled) {
        playTouchBeep();
    }
}

void HardwareManager::initPwm() {
    // Backlight PWM (5 kHz, 8-bit)
    ledcSetup(LEDC_BL_CH, 5000, 8);
    ledcAttachPin(CYD_TFT_BL, LEDC_BL_CH);

    // RGB LEDs (5 kHz, 8-bit)
    ledcSetup(LEDC_RED_CH, 5000, 8);
    ledcAttachPin(CYD_LED_RED, LEDC_RED_CH);

    ledcSetup(LEDC_GRN_CH, 5000, 8);
    ledcAttachPin(CYD_LED_GREEN, LEDC_GRN_CH);

    ledcSetup(LEDC_BLU_CH, 5000, 8);
    ledcAttachPin(CYD_LED_BLUE, LEDC_BLU_CH);

    // Buzzer PWM (2.5 kHz, 8-bit)
    ledcSetup(LEDC_BUZ_CH, 2400, 8);
    ledcAttachPin(CYD_BUZZER_PIN, LEDC_BUZ_CH);
    ledcWrite(LEDC_BUZ_CH, 0);
}

void HardwareManager::saveSettings() {
    m_prefs.putBool("cfg_init", true);
    m_prefs.putString("obd_name", m_settings.obdTargetName);
    m_prefs.putBool("imperial", m_settings.useImperial);
    m_prefs.putBool("demo", m_settings.demoMode);
    m_prefs.putBool("sound", m_settings.soundEnabled);
    m_prefs.putBool("auto_br", m_settings.autoBrightness);
    m_prefs.putUChar("man_br", m_settings.manualBrightness);
    m_prefs.putUShort("shift_rpm", m_settings.shiftRpm);
    m_prefs.putUShort("redline_rpm", m_settings.redlineRpm);
    m_prefs.putUChar("theme", m_settings.colorTheme);
    m_prefs.putBool("cycle", m_settings.autoCyclePages);
    m_prefs.putUChar("cycle_sec", m_settings.cycleSeconds);
    m_prefs.putUShort("t_min_x", m_settings.touchMinX);
    m_prefs.putUShort("t_max_x", m_settings.touchMaxX);
    m_prefs.putUShort("t_min_y", m_settings.touchMinY);
    m_prefs.putUShort("t_max_y", m_settings.touchMaxY);
}

void HardwareManager::resetSettings() {
    m_settings = AppSettings();
    saveSettings();
}

void HardwareManager::setLedColor(uint8_t r, uint8_t g, uint8_t b) {
    // CYD RGB LED is active-low: 0 is full ON, 255 is OFF
    ledcWrite(LEDC_RED_CH, 255 - r);
    ledcWrite(LEDC_GRN_CH, 255 - g);
    ledcWrite(LEDC_BLU_CH, 255 - b);
}

void HardwareManager::setBacklight(uint8_t brightnessPercent) {
    m_currentBrightness = constrain(brightnessPercent, (uint8_t)5, (uint8_t)100);
    // Convert 0-100% to 0-255 PWM duty
    uint8_t duty = (uint8_t)((m_currentBrightness * 255) / 100);
    ledcWrite(LEDC_BL_CH, duty);
}

void HardwareManager::update(const OBDMetrics& metrics) {
    unsigned long now = millis();

    // 1. Auto-brightness with LDR (sampled every 200ms)
    if (m_settings.autoBrightness && (now - m_lastLdrReadMs > 200)) {
        m_lastLdrReadMs = now;
        m_rawLdr = analogRead(CYD_LDR_PIN);
        // Exponential moving average filter
        m_smoothLdr = (m_smoothLdr * 0.85f) + ((float)m_rawLdr * 0.15f);

        // LDR: Dark ~ 3500-4095 (or vice versa), Bright ~ 200-1000
        // Map smoothLdr to 15% - 100% brightness
        // Lower ADC value on CYD = brighter ambient light
        int targetBr = map((int)m_smoothLdr, 4000, 400, 15, 100);
        targetBr = constrain(targetBr, 15, 100);
        setBacklight((uint8_t)targetBr);
    } else if (!m_settings.autoBrightness) {
        setBacklight(m_settings.manualBrightness);
    }

    // 2. Shift Light and Warning LED Updates
    updateShiftLight(metrics.rpm, m_settings.shiftRpm, m_settings.redlineRpm, metrics);

    // 3. Buzzer Auto-off
    if (m_buzzerActive && (now >= m_buzzerOffMs)) {
        ledcWrite(LEDC_BUZ_CH, 0);
        m_buzzerActive = false;
    }
}

void HardwareManager::updateShiftLight(float rpm, float shiftRpm, float redlineRpm, const OBDMetrics& metrics) {
    unsigned long now = millis();

    // High Priority 1: Coolant Overheat Warning (> 105°C) -> Flash Red
    if (metrics.coolantTempC >= DEFAULT_COOLANT_WARN_C) {
        bool flashOn = (now / 200) % 2 == 0;
        setLedColor(flashOn ? 255 : 0, 0, 0);
        return;
    }

    // High Priority 2: Battery Voltage Warning (< 11.8V or > 15.2V)
    if (metrics.batteryVoltage > 5.0f && (metrics.batteryVoltage < DEFAULT_VOLT_LOW_WARN || metrics.batteryVoltage > DEFAULT_VOLT_HIGH_WARN)) {
        bool flashOn = (now / 350) % 2 == 0;
        setLedColor(flashOn ? 255 : 0, flashOn ? 120 : 0, 0); // Flash Amber
        return;
    }

    // High Priority 3: Connection State
    if (metrics.state == OBD_STATE_CONNECTING_BT || metrics.state == OBD_STATE_INIT_ELM) {
        // Pulsing Blue
        uint8_t bluePulse = (uint8_t)(128.0f + 127.0f * sin((float)now / 200.0f));
        setLedColor(0, 0, bluePulse);
        return;
    }

    if (metrics.state == OBD_STATE_ERROR || metrics.state == OBD_STATE_DISCONNECTED) {
        // Slow Dim Red pulse
        uint8_t redPulse = (uint8_t)(40.0f + 30.0f * sin((float)now / 500.0f));
        setLedColor(redPulse, 0, 0);
        return;
    }

    // Dynamic Shift Light Engine
    float shiftRatio = (shiftRpm > 0) ? (rpm / shiftRpm) : 0.0f;

    if (rpm >= redlineRpm) {
        // Over-rev / Redline: Strobe White / Red rapidly
        bool strobe = (now / 60) % 2 == 0;
        if (strobe) {
            setLedColor(255, 255, 255);
        } else {
            setLedColor(255, 0, 0);
        }
        if (!m_shiftBeepTriggered && m_settings.soundEnabled) {
            playShiftBeep();
            m_shiftBeepTriggered = true;
        }
    } else if (rpm >= shiftRpm) {
        // Shift point reached: Solid Bright Red
        setLedColor(255, 0, 0);
        if (!m_shiftBeepTriggered && m_settings.soundEnabled) {
            playShiftBeep();
            m_shiftBeepTriggered = true;
        }
    } else if (shiftRatio >= 0.90f) {
        // Approaching shift (90-100%): Orange / Amber
        setLedColor(255, 120, 0);
        m_shiftBeepTriggered = false;
    } else if (shiftRatio >= 0.80f) {
        // 80-90%: Yellow
        setLedColor(200, 200, 0);
        m_shiftBeepTriggered = false;
    } else if (shiftRatio >= 0.65f) {
        // 65-80%: Green
        setLedColor(0, 200, 0);
        m_shiftBeepTriggered = false;
    } else {
        // Normal / Cruising / Idle: Very subtle Cyan or Off
        if (metrics.state == OBD_STATE_CONNECTED || metrics.isDemo) {
            setLedColor(0, 10, 20); // Subtle dashboard glow
        } else {
            setLedColor(0, 0, 0);
        }
        m_shiftBeepTriggered = false;
    }
}

void HardwareManager::playTouchBeep() {
    if (!m_settings.soundEnabled) return;
    ledcWriteTone(LEDC_BUZ_CH, 2200);
    ledcWrite(LEDC_BUZ_CH, 128);
    m_buzzerOffMs = millis() + 35;
    m_buzzerActive = true;
}

void HardwareManager::playShiftBeep() {
    if (!m_settings.soundEnabled) return;
    ledcWriteTone(LEDC_BUZ_CH, 1800);
    ledcWrite(LEDC_BUZ_CH, 180);
    m_buzzerOffMs = millis() + 80;
    m_buzzerActive = true;
}

void HardwareManager::playAlarmBeep() {
    if (!m_settings.soundEnabled) return;
    ledcWriteTone(LEDC_BUZ_CH, 3000);
    ledcWrite(LEDC_BUZ_CH, 200);
    m_buzzerOffMs = millis() + 150;
    m_buzzerActive = true;
}

void HardwareManager::mapTouchCoordinates(int16_t rawX, int16_t rawY, int16_t &outX, int16_t &outY) {
    // Map raw ADC touch coordinates to 320x240 screen pixels
    int32_t x = map(rawX, m_settings.touchMinX, m_settings.touchMaxX, 0, SCREEN_WIDTH);
    int32_t y = map(rawY, m_settings.touchMinY, m_settings.touchMaxY, 0, SCREEN_HEIGHT);
    outX = (int16_t)constrain(x, 0, SCREEN_WIDTH - 1);
    outY = (int16_t)constrain(y, 0, SCREEN_HEIGHT - 1);
}

bool HardwareManager::isTouched() {
    return m_touch.touched();
}

TouchEvent HardwareManager::getTouchEvent() {
    TouchEvent ev;
    unsigned long now = millis();
    bool currentlyPressed = m_touch.touched();

    if (currentlyPressed) {
        TS_Point p = m_touch.getPoint();
        int16_t mappedX = 0, mappedY = 0;
        mapTouchCoordinates(p.x, p.y, mappedX, mappedY);

        ev.isPressed = true;
        ev.x = mappedX;
        ev.y = mappedY;
        m_lastX = mappedX;
        m_lastY = mappedY;

        if (!m_wasPressed) {
            // Just touched down
            ev.justPressed = true;
            m_wasPressed = true;
            m_pressStartMs = now;
            m_pressStartX = mappedX;
            m_pressStartY = mappedY;
        }
    } else {
        if (m_wasPressed) {
            // Just released
            ev.justReleased = true;
            ev.x = m_lastX;
            ev.y = m_lastY;
            m_wasPressed = false;

            unsigned long pressDuration = now - m_pressStartMs;
            int16_t deltaX = m_lastX - m_pressStartX;
            int16_t deltaY = m_lastY - m_pressStartY;

            // Gesture recognition
            if (pressDuration < 700) {
                if (deltaX > 60 && abs(deltaY) < 80) {
                    ev.gesture = GESTURE_SWIPE_RIGHT;
                } else if (deltaX < -60 && abs(deltaY) < 80) {
                    ev.gesture = GESTURE_SWIPE_LEFT;
                } else if (abs(deltaX) < 30 && abs(deltaY) < 30) {
                    ev.gesture = GESTURE_TAP;
                    if (m_settings.soundEnabled) {
                        playTouchBeep();
                    }
                }
            }
        }
    }

    return ev;
}
