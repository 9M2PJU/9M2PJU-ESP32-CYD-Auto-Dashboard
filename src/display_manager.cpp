#include "display_manager.h"
#include "obd_engine.h"
#include <math.h>

DisplayManager Display;

DisplayManager::DisplayManager() : m_tft(TFT_eSPI()) {
}

void DisplayManager::begin() {
    m_tft.init();
    m_tft.setRotation(1); // Landscape 320x240
    m_tft.fillScreen(COLOR_BG);
    m_pageNeedsFullRedraw = true;
}

void DisplayManager::setPage(DashboardPage page) {
    if (m_currentPage != page) {
        m_currentPage = page;
        m_pageNeedsFullRedraw = true;
        m_lastPageChangeMs = millis();
    }
}

void DisplayManager::nextPage() {
    int next = ((int)m_currentPage + 1) % PAGE_COUNT;
    setPage((DashboardPage)next);
}

void DisplayManager::prevPage() {
    int prev = ((int)m_currentPage - 1 + PAGE_COUNT) % PAGE_COUNT;
    setPage((DashboardPage)prev);
}

void DisplayManager::update(const OBDMetrics& metrics, const PerformanceTracker& perf, const TouchEvent& touch) {
    // 1. Process touch and gestures
    handleTouch(touch, metrics);

    // 2. Auto-cycle pages if enabled in settings
    AppSettings& cfg = Hardware.getSettings();
    if (cfg.autoCyclePages && !m_showConfirmModal) {
        unsigned long cycleIntervalMs = (unsigned long)cfg.cycleSeconds * 1000;
        if (millis() - m_lastPageChangeMs > cycleIntervalMs) {
            nextPage();
        }
    }

    // 3. Page redrawing logic
    if (m_currentPage != m_lastRenderedPage || m_pageNeedsFullRedraw) {
        m_tft.fillScreen(COLOR_BG);
        renderTopBar(metrics);
        renderNavBar();
        m_lastRenderedPage = m_currentPage;
        m_pageNeedsFullRedraw = false;
    } else {
        renderTopBar(metrics);
    }

    // 4. Render Active Page
    switch (m_currentPage) {
        case PAGE_SPORT_CLUSTER:
            renderSportCluster(metrics);
            break;
        case PAGE_TELEMETRY_GRID:
            renderTelemetryGrid(metrics);
            break;
        case PAGE_PERFORMANCE:
            renderPerformance(metrics, perf);
            break;
        case PAGE_DIAGNOSTICS:
            renderDiagnostics(metrics);
            break;
        case PAGE_SETTINGS:
            renderSettings(metrics);
            break;
        default:
            break;
    }

    // 5. Render Modal if open
    if (m_showConfirmModal) {
        renderConfirmModal();
    }
}

// ============================================================================
// Top Status Bar & Navigation Bar
// ============================================================================

void DisplayManager::renderTopBar(const OBDMetrics& metrics) {
    // Background bar
    m_tft.fillRect(0, 0, SCREEN_WIDTH, 22, 0x10A2);
    m_tft.drawFastHLine(0, 22, SCREEN_WIDTH, COLOR_CARD_BORDER);

    // Left: Connection Status Dot & Label
    uint16_t stateColor = COLOR_ACCENT_RED;
    const char* stateText = "DISCONNECTED";

    if (metrics.isDemo) {
        stateColor = COLOR_ACCENT_AMBER;
        stateText = "DEMO SIM";
    } else {
        switch (metrics.state) {
            case OBD_STATE_CONNECTED:
                stateColor = COLOR_ACCENT_GRN;
                stateText = "OBD CONNECTED";
                break;
            case OBD_STATE_CONNECTING_BT:
                stateColor = COLOR_ACCENT_BLUE;
                stateText = "BT CONNECTING...";
                break;
            case OBD_STATE_INIT_ELM:
                stateColor = COLOR_ACCENT_CYAN;
                stateText = "INIT ELM327...";
                break;
            default:
                stateColor = COLOR_ACCENT_RED;
                stateText = "OFFLINE";
                break;
        }
    }

    // Draw status dot
    m_tft.fillCircle(8, 11, 4, stateColor);
    m_tft.setTextColor(COLOR_TEXT_BRIGHT, 0x10A2);
    m_tft.drawString(stateText, 18, 5, 2);

    // Center: Shift RPM / Page Label
    char centerBuf[24];
    if (metrics.coolantTempC >= DEFAULT_COOLANT_WARN_C) {
        m_tft.setTextColor(COLOR_ACCENT_RED, 0x10A2);
        snprintf(centerBuf, sizeof(centerBuf), "! HIGH TEMP !");
    } else {
        m_tft.setTextColor(COLOR_TEXT_DIM, 0x10A2);
        snprintf(centerBuf, sizeof(centerBuf), "9M2PJU CYD");
    }
    m_tft.drawCentreString(centerBuf, 160, 5, 2);

    // Right: Battery Voltage Badge
    char voltBuf[16];
    snprintf(voltBuf, sizeof(voltBuf), "%.1fV", metrics.batteryVoltage);
    uint16_t voltColor = COLOR_ACCENT_GRN;
    if (metrics.batteryVoltage < DEFAULT_VOLT_LOW_WARN) {
        voltColor = COLOR_ACCENT_RED;
    } else if (metrics.batteryVoltage < 12.3f) {
        voltColor = COLOR_ACCENT_AMBER;
    }
    m_tft.setTextColor(voltColor, 0x10A2);
    m_tft.drawRightString(voltBuf, 314, 5, 2);
}

void DisplayManager::renderNavBar() {
    int navY = 214;
    int tabW = 64; // 320 / 5 = 64px each

    m_tft.fillRect(0, navY, SCREEN_WIDTH, 26, 0x10A2);
    m_tft.drawFastHLine(0, navY, SCREEN_WIDTH, COLOR_CARD_BORDER);

    const char* tabs[PAGE_COUNT] = {"SPORT", "GRID", "DRAG", "DIAG", "SETUP"};

    for (int i = 0; i < PAGE_COUNT; i++) {
        int x = i * tabW;
        bool isActive = (i == (int)m_currentPage);

        if (isActive) {
            m_tft.fillRect(x + 2, navY + 2, tabW - 4, 22, 0x2124);
            m_tft.drawFastHLine(x + 4, navY + 23, tabW - 8, COLOR_ACCENT_CYAN);
            m_tft.setTextColor(COLOR_ACCENT_CYAN, 0x2124);
        } else {
            m_tft.setTextColor(COLOR_TEXT_DIM, 0x10A2);
        }
        m_tft.drawCentreString(tabs[i], x + (tabW / 2), navY + 6, 2);
    }
}

// ============================================================================
// Page 0: Sport Cluster Dashboard
// ============================================================================

void DisplayManager::renderSportCluster(const OBDMetrics& metrics) {
    AppSettings& cfg = Hardware.getSettings();

    // 1. Center Sweeping Tachometer Arc (Center X: 160, Y: 110)
    int16_t cx = 160;
    int16_t cy = 110;
    int16_t r = 78;

    // Draw RPM Outer Track Background
    drawArcBar(cx, cy, r, 10, 135, 405, 0x18E3);

    // Calculate RPM progress angle
    float maxRpm = (float)DEFAULT_MAX_RPM;
    float rpmClamped = constrain(metrics.rpm, 0.0f, maxRpm);
    float rpmAngle = 135.0f + (rpmClamped / maxRpm) * 270.0f;

    // Color gradient based on RPM zones
    uint16_t rpmColor = COLOR_ACCENT_CYAN;
    if (metrics.rpm >= cfg.redlineRpm) {
        rpmColor = COLOR_ACCENT_RED;
    } else if (metrics.rpm >= cfg.shiftRpm) {
        rpmColor = COLOR_ACCENT_AMBER;
    } else if (metrics.rpm >= 3500) {
        rpmColor = COLOR_ACCENT_GRN;
    }

    drawArcBar(cx, cy, r, 10, 135, rpmAngle, rpmColor);

    // 2. Center Speedometer Display
    char speedBuf[16];
    float dispSpeed = metrics.getSpeed(cfg.useImperial);
    snprintf(speedBuf, sizeof(speedBuf), "%d", (int)round(dispSpeed));

    m_tft.setTextColor(COLOR_TEXT_BRIGHT, COLOR_BG);
    m_tft.drawCentreString(speedBuf, cx, cy - 28, 7); // Large 7-segment / Big Font

    // Speed Unit Label
    m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    m_tft.drawCentreString(metrics.getSpeedUnit(cfg.useImperial), cx, cy + 22, 2);

    // 3. Digital RPM Readout
    char rpmBuf[24];
    snprintf(rpmBuf, sizeof(rpmBuf), "%4d RPM", (int)round(metrics.rpm));
    m_tft.setTextColor(rpmColor, COLOR_BG);
    m_tft.drawCentreString(rpmBuf, cx, cy + 38, 4);

    // 4. Center Gear Badge
    drawGearBadge(cx - 15, cy + 68, metrics.estimatedGear);

    // 5. Left Side: Coolant Temperature Card & Bar
    int leftX = 8;
    int leftY = 32;
    int cardW = 68;
    int cardH = 168;

    m_tft.fillRoundRect(leftX, leftY, cardW, cardH, 4, COLOR_CARD_BG);
    m_tft.drawRoundRect(leftX, leftY, cardW, cardH, 4, COLOR_CARD_BORDER);

    m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
    m_tft.drawCentreString("COOLANT", leftX + (cardW / 2), leftY + 6, 2);

    float dispCoolant = metrics.getCoolantTemp(cfg.useImperial);
    char coolBuf[16];
    snprintf(coolBuf, sizeof(coolBuf), "%d%s", (int)round(dispCoolant), metrics.getTempUnit(cfg.useImperial));

    uint16_t coolColor = COLOR_ACCENT_CYAN;
    if (metrics.coolantTempC >= DEFAULT_COOLANT_WARN_C) {
        coolColor = COLOR_ACCENT_RED;
    } else if (metrics.coolantTempC >= 95.0f) {
        coolColor = COLOR_ACCENT_AMBER;
    } else if (metrics.coolantTempC < 65.0f) {
        coolColor = COLOR_ACCENT_BLUE; // Cold engine
    }

    m_tft.setTextColor(coolColor, COLOR_CARD_BG);
    m_tft.drawCentreString(coolBuf, leftX + (cardW / 2), leftY + 24, 4);

    // Vertical Coolant Bar
    float coolPercent = constrain((metrics.coolantTempC - 40.0f) / (120.0f - 40.0f), 0.0f, 1.0f);
    int barX = leftX + 16;
    int barY = leftY + 54;
    int barW = 12;
    int barH = 88;

    m_tft.fillRect(barX, barY, barW, barH, 0x10A2);
    int fillH = (int)(barH * coolPercent);
    m_tft.fillRect(barX, barY + (barH - fillH), barW, fillH, coolColor);
    m_tft.drawRect(barX, barY, barW, barH, COLOR_CARD_BORDER);

    m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
    m_tft.drawString("H", barX + 16, barY, 2);
    m_tft.drawString("C", barX + 16, barY + barH - 12, 2);

    // Intake Temp Mini Readout at bottom of left card
    float dispIntake = metrics.getIntakeTemp(cfg.useImperial);
    char iatBuf[16];
    snprintf(iatBuf, sizeof(iatBuf), "IAT:%d%s", (int)round(dispIntake), metrics.getTempUnit(cfg.useImperial));
    m_tft.drawCentreString(iatBuf, leftX + (cardW / 2), leftY + 148, 1);

    // 6. Right Side: Throttle & Engine Load Card
    int rightX = 244;
    int rightY = 32;

    m_tft.fillRoundRect(rightX, rightY, cardW, cardH, 4, COLOR_CARD_BG);
    m_tft.drawRoundRect(rightX, rightY, cardW, cardH, 4, COLOR_CARD_BORDER);

    m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
    m_tft.drawCentreString("THROTTLE", rightX + (cardW / 2), rightY + 6, 2);

    char tpsBuf[16];
    snprintf(tpsBuf, sizeof(tpsBuf), "%d%%", (int)round(metrics.throttlePos));
    m_tft.setTextColor(COLOR_ACCENT_GRN, COLOR_CARD_BG);
    m_tft.drawCentreString(tpsBuf, rightX + (cardW / 2), rightY + 24, 4);

    // Dual Vertical Meters: Throttle (Left) & Load (Right)
    int tpsBarX = rightX + 14;
    int loadBarX = rightX + 40;
    int meterY = rightY + 54;
    int meterW = 12;
    int meterH = 88;

    // TPS Bar
    float tpsPercent = constrain(metrics.throttlePos / 100.0f, 0.0f, 1.0f);
    m_tft.fillRect(tpsBarX, meterY, meterW, meterH, 0x10A2);
    int tpsFillH = (int)(meterH * tpsPercent);
    m_tft.fillRect(tpsBarX, meterY + (meterH - tpsFillH), meterW, tpsFillH, COLOR_ACCENT_GRN);
    m_tft.drawRect(tpsBarX, meterY, meterW, meterH, COLOR_CARD_BORDER);

    // Load Bar
    float loadPercent = constrain(metrics.engineLoad / 100.0f, 0.0f, 1.0f);
    m_tft.fillRect(loadBarX, meterY, meterW, meterH, 0x10A2);
    int loadFillH = (int)(meterH * loadPercent);
    uint16_t loadCol = (metrics.engineLoad > 85.0f) ? COLOR_ACCENT_RED : COLOR_ACCENT_AMBER;
    m_tft.fillRect(loadBarX, meterY + (meterH - loadFillH), meterW, loadFillH, loadCol);
    m_tft.drawRect(loadBarX, meterY, meterW, meterH, COLOR_CARD_BORDER);

    char loadBuf[16];
    snprintf(loadBuf, sizeof(loadBuf), "LOD:%d%%", (int)round(metrics.engineLoad));
    m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
    m_tft.drawCentreString(loadBuf, rightX + (cardW / 2), rightY + 148, 1);
}

// ============================================================================
// Page 1: Telemetry 6-Card Grid
// ============================================================================

void DisplayManager::renderTelemetryGrid(const OBDMetrics& metrics) {
    AppSettings& cfg = Hardware.getSettings();

    int cardW = 98;
    int cardH = 82;
    int startY = 30;

    // Row 1
    // Card 1: Coolant Temp
    drawCard(8, startY, cardW, cardH, "COOLANT TEMP");
    float coolDisp = metrics.getCoolantTemp(cfg.useImperial);
    char buf[24];
    snprintf(buf, sizeof(buf), "%d %s", (int)round(coolDisp), metrics.getTempUnit(cfg.useImperial));
    uint16_t coolCol = (metrics.coolantTempC >= DEFAULT_COOLANT_WARN_C) ? COLOR_ACCENT_RED : COLOR_ACCENT_CYAN;
    m_tft.setTextColor(coolCol, COLOR_CARD_BG);
    m_tft.drawCentreString(buf, 8 + (cardW / 2), startY + 26, 4);
    drawProgressBar(8 + 8, startY + 60, cardW - 16, 6, constrain((metrics.coolantTempC - 40.0f) / 80.0f, 0.0f, 1.0f), coolCol, 0x10A2);

    // Card 2: Intake Air Temp
    drawCard(111, startY, cardW, cardH, "INTAKE TEMP");
    float iatDisp = metrics.getIntakeTemp(cfg.useImperial);
    snprintf(buf, sizeof(buf), "%d %s", (int)round(iatDisp), metrics.getTempUnit(cfg.useImperial));
    m_tft.setTextColor(COLOR_TEXT_BRIGHT, COLOR_CARD_BG);
    m_tft.drawCentreString(buf, 111 + (cardW / 2), startY + 26, 4);
    drawProgressBar(111 + 8, startY + 60, cardW - 16, 6, constrain(metrics.intakeAirTempC / 70.0f, 0.0f, 1.0f), COLOR_ACCENT_BLUE, 0x10A2);

    // Card 3: Battery Voltage
    drawCard(214, startY, cardW, cardH, "BATTERY VOLTS");
    snprintf(buf, sizeof(buf), "%.1f V", metrics.batteryVoltage);
    uint16_t voltCol = (metrics.batteryVoltage < DEFAULT_VOLT_LOW_WARN) ? COLOR_ACCENT_RED : COLOR_ACCENT_GRN;
    m_tft.setTextColor(voltCol, COLOR_CARD_BG);
    m_tft.drawCentreString(buf, 214 + (cardW / 2), startY + 26, 4);
    drawProgressBar(214 + 8, startY + 60, cardW - 16, 6, constrain((metrics.batteryVoltage - 11.0f) / 4.0f, 0.0f, 1.0f), voltCol, 0x10A2);

    // Row 2
    int startY2 = 120;

    // Card 4: Engine Load
    drawCard(8, startY2, cardW, cardH, "ENGINE LOAD");
    snprintf(buf, sizeof(buf), "%d %%", (int)round(metrics.engineLoad));
    m_tft.setTextColor(COLOR_ACCENT_AMBER, COLOR_CARD_BG);
    m_tft.drawCentreString(buf, 8 + (cardW / 2), startY2 + 26, 4);
    drawProgressBar(8 + 8, startY2 + 60, cardW - 16, 6, metrics.engineLoad / 100.0f, COLOR_ACCENT_AMBER, 0x10A2);

    // Card 5: Throttle Position
    drawCard(111, startY2, cardW, cardH, "THROTTLE POS");
    snprintf(buf, sizeof(buf), "%d %%", (int)round(metrics.throttlePos));
    m_tft.setTextColor(COLOR_ACCENT_GRN, COLOR_CARD_BG);
    m_tft.drawCentreString(buf, 111 + (cardW / 2), startY2 + 26, 4);
    drawProgressBar(111 + 8, startY2 + 60, cardW - 16, 6, metrics.throttlePos / 100.0f, COLOR_ACCENT_GRN, 0x10A2);

    // Card 6: Boost / MAP
    drawCard(214, startY2, cardW, cardH, "MANIFOLD / BOOST");
    float boostPsi = metrics.getBoostPsi();
    if (boostPsi > 0.5f) {
        snprintf(buf, sizeof(buf), "+%.1f PSI", boostPsi);
        m_tft.setTextColor(COLOR_ACCENT_RED, COLOR_CARD_BG);
    } else {
        snprintf(buf, sizeof(buf), "%d kPa", (int)round(metrics.mapKpa));
        m_tft.setTextColor(COLOR_ACCENT_CYAN, COLOR_CARD_BG);
    }
    m_tft.drawCentreString(buf, 214 + (cardW / 2), startY2 + 26, 4);
    drawProgressBar(214 + 8, startY2 + 60, cardW - 16, 6, constrain(metrics.mapKpa / 200.0f, 0.0f, 1.0f), COLOR_ACCENT_CYAN, 0x10A2);
}

// ============================================================================
// Page 2: Performance & Drag Acceleration Timer
// ============================================================================

void DisplayManager::renderPerformance(const OBDMetrics& metrics, const PerformanceTracker& perf) {
    AppSettings& cfg = Hardware.getSettings();

    // 1. Status Banner
    const char* statusText = "STANDSTILL - READY FOR LAUNCH";
    uint16_t statusColor = COLOR_ACCENT_GRN;

    switch (perf.getState()) {
        case PERF_STATE_ARMED:
            statusText = "ARMED - HIT THROTTLE TO START!";
            statusColor = COLOR_ACCENT_GRN;
            break;
        case PERF_STATE_RUNNING:
            statusText = ">>> TIMING RUN IN PROGRESS <<<";
            statusColor = COLOR_ACCENT_RED;
            break;
        case PERF_STATE_FINISHED:
            statusText = "RUN COMPLETE - RESULTS READY";
            statusColor = COLOR_ACCENT_CYAN;
            break;
        default:
            statusText = "STOP CAR TO ARM TIMER";
            statusColor = COLOR_TEXT_DIM;
            break;
    }

    m_tft.fillRect(8, 28, 304, 22, 0x18E3);
    m_tft.drawRect(8, 28, 304, 22, COLOR_CARD_BORDER);
    m_tft.setTextColor(statusColor, 0x18E3);
    m_tft.drawCentreString(statusText, 160, 32, 2);

    // 2. Large Live Elapsed Time
    char timeBuf[24];
    snprintf(timeBuf, sizeof(timeBuf), "%.2f s", perf.getLiveTimeSec());
    m_tft.setTextColor(COLOR_TEXT_BRIGHT, COLOR_BG);
    m_tft.drawCentreString(timeBuf, 160, 56, 6);

    // 3. Telemetry Splits Grid (3 Columns)
    int splitY = 104;
    int splitW = 96;
    int splitH = 58;

    // Split 1: 0 - 60 km/h
    drawCard(8, splitY, splitW, splitH, "0 - 60 KM/H");
    char s60Buf[16];
    if (perf.getCurrentRun().time0to60 > 0.0f) {
        snprintf(s60Buf, sizeof(s60Buf), "%.2f s", perf.getCurrentRun().time0to60);
        m_tft.setTextColor(COLOR_ACCENT_CYAN, COLOR_CARD_BG);
    } else {
        snprintf(s60Buf, sizeof(s60Buf), "--.--");
        m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
    }
    m_tft.drawCentreString(s60Buf, 8 + (splitW / 2), splitY + 24, 4);

    // Split 2: 0 - 100 km/h (The standard 0-60mph metric)
    drawCard(112, splitY, splitW, splitH, "0 - 100 KM/H");
    char s100Buf[16];
    if (perf.getCurrentRun().time0to100 > 0.0f) {
        snprintf(s100Buf, sizeof(s100Buf), "%.2f s", perf.getCurrentRun().time0to100);
        m_tft.setTextColor(COLOR_ACCENT_GRN, COLOR_CARD_BG);
    } else {
        snprintf(s100Buf, sizeof(s100Buf), "--.--");
        m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
    }
    m_tft.drawCentreString(s100Buf, 112 + (splitW / 2), splitY + 24, 4);

    // Split 3: Peak G-Force / Best 0-100
    drawCard(216, splitY, splitW, splitH, "PEAK G / BEST");
    char bestBuf[16];
    if (perf.getBestRun().time0to100 > 0.0f) {
        snprintf(bestBuf, sizeof(bestBuf), "%.2fs (%.1fG)", perf.getBestRun().time0to100, perf.getCurrentRun().peakGForce);
    } else {
        snprintf(bestBuf, sizeof(bestBuf), "%.2f G", perf.getCurrentRun().peakGForce);
    }
    m_tft.setTextColor(COLOR_ACCENT_AMBER, COLOR_CARD_BG);
    m_tft.drawCentreString(bestBuf, 216 + (splitW / 2), splitY + 24, 2);

    // 4. Action Button: ARM / RESET
    drawButton(80, 170, 160, 34, "RESET / RE-ARM TIMER", 0x2186, COLOR_TEXT_BRIGHT);
}

// ============================================================================
// Page 3: Diagnostics & DTC Scanner
// ============================================================================

void DisplayManager::renderDiagnostics(const OBDMetrics& metrics) {
    // 1. MIL / Check Engine Header Card
    int milY = 28;
    int milH = 34;

    uint16_t milBg = metrics.milStatus ? 0x6000 : 0x0320;
    uint16_t milTextCol = metrics.milStatus ? COLOR_ACCENT_RED : COLOR_ACCENT_GRN;
    const char* milTitle = metrics.milStatus ? "CHECK ENGINE LIGHT: ACTIVE (MIL ON)" : "CHECK ENGINE LIGHT: NORMAL (MIL OFF)";

    m_tft.fillRoundRect(8, milY, 304, milH, 4, milBg);
    m_tft.drawRoundRect(8, milY, 304, milH, 4, milTextCol);
    m_tft.setTextColor(milTextCol, milBg);
    m_tft.drawCentreString(milTitle, 160, milY + 8, 2);

    // 2. Fault Codes List Container
    int listY = 68;
    int listH = 96;
    m_tft.fillRoundRect(8, listY, 304, listH, 4, COLOR_CARD_BG);
    m_tft.drawRoundRect(8, listY, 304, listH, 4, COLOR_CARD_BORDER);

    if (metrics.dtcCodes.empty()) {
        m_tft.setTextColor(COLOR_ACCENT_GRN, COLOR_CARD_BG);
        m_tft.drawCentreString("NO STORED TROUBLE CODES DETECTED", 160, listY + 36, 2);
        m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
        m_tft.drawCentreString("ECU diagnostic status: OK", 160, listY + 56, 1);
    } else {
        m_tft.setTextColor(COLOR_ACCENT_AMBER, COLOR_CARD_BG);
        char hdrBuf[32];
        snprintf(hdrBuf, sizeof(hdrBuf), "FOUND %d FAULT CODE(S):", (int)metrics.dtcCodes.size());
        m_tft.drawString(hdrBuf, 16, listY + 6, 2);

        // Display up to 3 codes on screen
        int itemY = listY + 26;
        for (size_t i = 0; i < metrics.dtcCodes.size() && i < 3; i++) {
            String code = metrics.dtcCodes[i];
            String desc = Obd.getDtcDescription(code);

            m_tft.setTextColor(COLOR_ACCENT_RED, COLOR_CARD_BG);
            m_tft.drawString(code, 16, itemY, 2);

            m_tft.setTextColor(COLOR_TEXT_BRIGHT, COLOR_CARD_BG);
            m_tft.drawString(desc.substring(0, 32), 80, itemY, 1);
            itemY += 20;
        }
    }

    // 3. Action Buttons: SCAN DTCs and CLEAR CODES
    drawButton(16, 172, 136, 34, "SCAN DTCs", 0x1A4F, COLOR_TEXT_BRIGHT);
    drawButton(168, 172, 136, 34, "CLEAR CODES", 0x8000, COLOR_TEXT_BRIGHT);
}

// ============================================================================
// Page 4: Settings & Configuration
// ============================================================================

void DisplayManager::renderSettings(const OBDMetrics& metrics) {
    AppSettings& cfg = Hardware.getSettings();

    int rowY = 28;
    int rowH = 32;
    int pad = 4;

    // Row 1: Demo Mode & Units
    // Demo Mode Button
    uint16_t demoBg = cfg.demoMode ? 0x8200 : 0x18E3;
    const char* demoLabel = cfg.demoMode ? "DEMO: ON (SIM)" : "DEMO: OFF (OBD)";
    drawButton(8, rowY, 148, rowH, demoLabel, demoBg, COLOR_TEXT_BRIGHT);

    // Units Button
    const char* unitLabel = cfg.useImperial ? "UNITS: MPH / °F" : "UNITS: KM/H / °C";
    drawButton(164, rowY, 148, rowH, unitLabel, 0x18E3, COLOR_TEXT_BRIGHT);

    // Row 2: Shift RPM setting with - / + buttons
    int row2Y = rowY + rowH + pad;
    m_tft.fillRoundRect(8, row2Y, 304, rowH, 4, COLOR_CARD_BG);
    m_tft.drawRoundRect(8, row2Y, 304, rowH, 4, COLOR_CARD_BORDER);

    drawButton(12, row2Y + 3, 40, rowH - 6, " - ", 0x2965, COLOR_TEXT_BRIGHT);
    char rpmBuf[32];
    snprintf(rpmBuf, sizeof(rpmBuf), "SHIFT RPM: %d", cfg.shiftRpm);
    m_tft.setTextColor(COLOR_ACCENT_CYAN, COLOR_CARD_BG);
    m_tft.drawCentreString(rpmBuf, 160, row2Y + 8, 2);
    drawButton(268, row2Y + 3, 40, rowH - 6, " + ", 0x2965, COLOR_TEXT_BRIGHT);

    // Row 3: Brightness Mode & Sound Toggle
    int row3Y = row2Y + rowH + pad;
    const char* brLabel = cfg.autoBrightness ? "BRIGHTNESS: AUTO" : "BRIGHTNESS: MANUAL";
    drawButton(8, row3Y, 148, rowH, brLabel, 0x18E3, COLOR_TEXT_BRIGHT);

    const char* sndLabel = cfg.soundEnabled ? "SOUND: ON" : "SOUND: MUTE";
    drawButton(164, row3Y, 148, rowH, sndLabel, cfg.soundEnabled ? 0x18E3 : 0x4208, COLOR_TEXT_BRIGHT);

    // Row 4: Target OBD Bluetooth Name & Reconnect
    int row4Y = row3Y + rowH + pad;
    char btBuf[32];
    snprintf(btBuf, sizeof(btBuf), "OBD BT: %s", cfg.obdTargetName);
    drawButton(8, row4Y, 148, rowH, btBuf, 0x18E3, COLOR_ACCENT_AMBER);

    drawButton(164, row4Y, 148, rowH, "RECONNECT OBD", 0x1A4F, COLOR_TEXT_BRIGHT);

    // Row 5: Touch Calibration & Reset
    int row5Y = row4Y + rowH + pad;
    drawButton(8, row5Y, 148, 24, "AUTO-CYCLE PAGES", cfg.autoCyclePages ? 0x0320 : 0x18E3, COLOR_TEXT_BRIGHT);
    drawButton(164, row5Y, 148, 24, "RESET DEFAULTS", 0x4000, COLOR_TEXT_BRIGHT);
}

// ============================================================================
// Modal Confirmation Dialog (For Clear DTCs)
// ============================================================================

void DisplayManager::renderConfirmModal() {
    int mw = 260;
    int mh = 130;
    int mx = (SCREEN_WIDTH - mw) / 2;
    int my = (SCREEN_HEIGHT - mh) / 2;

    // Dark overlay & modal box
    m_tft.fillRoundRect(mx, my, mw, mh, 8, 0x18C3);
    m_tft.drawRoundRect(mx, my, mw, mh, 8, COLOR_ACCENT_RED);

    m_tft.setTextColor(COLOR_ACCENT_RED, 0x18C3);
    m_tft.drawCentreString("CLEAR ALL FAULT CODES?", 160, my + 14, 2);

    m_tft.setTextColor(COLOR_TEXT_DIM, 0x18C3);
    m_tft.drawCentreString("This will reset ECU DTCs and MIL.", 160, my + 38, 1);
    m_tft.drawCentreString("Turn ignition ON, engine OFF.", 160, my + 52, 1);

    // Modal Buttons: CANCEL and CONFIRM
    drawButton(mx + 16, my + 80, 100, 34, "CANCEL", 0x3186, COLOR_TEXT_BRIGHT);
    drawButton(mx + 144, my + 80, 100, 34, "YES, CLEAR", 0x8000, COLOR_TEXT_BRIGHT);
}

// ============================================================================
// Touch Event Handlers
// ============================================================================

void DisplayManager::handleTouch(const TouchEvent& touch, const OBDMetrics& metrics) {
    if (!touch.justReleased) return;

    int tx = touch.x;
    int ty = touch.y;

    // 1. Gesture Swipes (Swipe Left = Next Page, Swipe Right = Prev Page)
    if (touch.gesture == GESTURE_SWIPE_LEFT) {
        nextPage();
        return;
    } else if (touch.gesture == GESTURE_SWIPE_RIGHT) {
        prevPage();
        return;
    }

    // 2. Modal Interception
    if (m_showConfirmModal) {
        int mw = 260;
        int mh = 130;
        int mx = (SCREEN_WIDTH - mw) / 2;
        int my = (SCREEN_HEIGHT - mh) / 2;

        // Cancel button
        if (tx >= mx + 16 && tx <= mx + 116 && ty >= my + 80 && ty <= my + 114) {
            m_showConfirmModal = false;
            m_pageNeedsFullRedraw = true;
        }
        // Yes, Clear button
        else if (tx >= mx + 144 && tx <= mx + 244 && ty >= my + 80 && ty <= my + 114) {
            Obd.clearDtcCodes();
            m_showConfirmModal = false;
            m_pageNeedsFullRedraw = true;
        }
        return;
    }

    // 3. Bottom Navigation Bar Clicks (Y >= 214)
    if (ty >= 214) {
        int tabIndex = tx / 64; // 320 / 5 = 64
        if (tabIndex >= 0 && tabIndex < PAGE_COUNT) {
            setPage((DashboardPage)tabIndex);
        }
        return;
    }

    // 4. Per-Page Touch Handlers
    switch (m_currentPage) {
        case PAGE_SPORT_CLUSTER:
            handleTouchSport(touch);
            break;
        case PAGE_TELEMETRY_GRID:
            handleTouchGrid(touch);
            break;
        case PAGE_PERFORMANCE:
            handleTouchPerformance(touch);
            break;
        case PAGE_DIAGNOSTICS:
            handleTouchDiagnostics(touch);
            break;
        case PAGE_SETTINGS:
            handleTouchSettings(touch);
            break;
        default:
            break;
    }
}

void DisplayManager::handleTouchSport(const TouchEvent& touch) {
    // Tap center speedometer to toggle KM/H / MPH
    if (touch.x >= 100 && touch.x <= 220 && touch.y >= 50 && touch.y <= 160) {
        AppSettings& cfg = Hardware.getSettings();
        cfg.useImperial = !cfg.useImperial;
        Hardware.saveSettings();
        m_pageNeedsFullRedraw = true;
    }
}

void DisplayManager::handleTouchGrid(const TouchEvent& touch) {
    // Tap any grid card to toggle units
    AppSettings& cfg = Hardware.getSettings();
    cfg.useImperial = !cfg.useImperial;
    Hardware.saveSettings();
    m_pageNeedsFullRedraw = true;
}

void DisplayManager::handleTouchPerformance(const TouchEvent& touch) {
    // Tap "RESET / RE-ARM TIMER" button (X: 80-240, Y: 170-204)
    if (touch.x >= 80 && touch.x <= 240 && touch.y >= 170 && touch.y <= 204) {
        PerfTracker.arm();
        m_pageNeedsFullRedraw = true;
    }
}

void DisplayManager::handleTouchDiagnostics(const TouchEvent& touch) {
    // "SCAN DTCs" button (X: 16-152, Y: 172-206)
    if (touch.x >= 16 && touch.x <= 152 && touch.y >= 172 && touch.y <= 206) {
        Obd.scanDtcCodes();
        m_pageNeedsFullRedraw = true;
    }
    // "CLEAR CODES" button (X: 168-304, Y: 172-206)
    else if (touch.x >= 168 && touch.x <= 304 && touch.y >= 172 && touch.y <= 206) {
        m_showConfirmModal = true;
        m_pageNeedsFullRedraw = true;
    }
}

void DisplayManager::handleTouchSettings(const TouchEvent& touch) {
    AppSettings& cfg = Hardware.getSettings();
    int tx = touch.x;
    int ty = touch.y;

    int rowY = 28;
    int rowH = 32;
    int pad = 4;

    // Row 1: Demo Mode (X: 8-156, Y: 28-60) & Units (X: 164-312, Y: 28-60)
    if (ty >= rowY && ty <= rowY + rowH) {
        if (tx >= 8 && tx <= 156) {
            cfg.demoMode = !cfg.demoMode;
            Obd.toggleDemoMode(cfg.demoMode);
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        } else if (tx >= 164 && tx <= 312) {
            cfg.useImperial = !cfg.useImperial;
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        }
    }

    // Row 2: Shift RPM Adjuster
    int row2Y = rowY + rowH + pad;
    if (ty >= row2Y && ty <= row2Y + rowH) {
        if (tx >= 12 && tx <= 52) { // Minus button
            if (cfg.shiftRpm > 3000) cfg.shiftRpm -= 250;
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        } else if (tx >= 268 && tx <= 308) { // Plus button
            if (cfg.shiftRpm < 8500) cfg.shiftRpm += 250;
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        }
    }

    // Row 3: Auto-Brightness (X: 8-156) & Sound (X: 164-312)
    int row3Y = row2Y + rowH + pad;
    if (ty >= row3Y && ty <= row3Y + rowH) {
        if (tx >= 8 && tx <= 156) {
            cfg.autoBrightness = !cfg.autoBrightness;
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        } else if (tx >= 164 && tx <= 312) {
            cfg.soundEnabled = !cfg.soundEnabled;
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        }
    }

    // Row 4: BT Target Name (X: 8-156) & Reconnect (X: 164-312)
    int row4Y = row3Y + rowH + pad;
    if (ty >= row4Y && ty <= row4Y + rowH) {
        if (tx >= 8 && tx <= 156) {
            // Cycle through common OBD names ("OBDII", "Viecar", "vGate", "OBDLink")
            if (strcmp(cfg.obdTargetName, "OBDII") == 0) {
                strncpy(cfg.obdTargetName, "Viecar", sizeof(cfg.obdTargetName));
            } else if (strcmp(cfg.obdTargetName, "Viecar") == 0) {
                strncpy(cfg.obdTargetName, "vGate", sizeof(cfg.obdTargetName));
            } else if (strcmp(cfg.obdTargetName, "vGate") == 0) {
                strncpy(cfg.obdTargetName, "OBDLink", sizeof(cfg.obdTargetName));
            } else {
                strncpy(cfg.obdTargetName, "OBDII", sizeof(cfg.obdTargetName));
            }
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        } else if (tx >= 164 && tx <= 312) {
            Obd.reconnect(cfg.obdTargetName);
            m_pageNeedsFullRedraw = true;
        }
    }

    // Row 5: Auto-Cycle (X: 8-156) & Reset (X: 164-312)
    int row5Y = row4Y + rowH + pad;
    if (ty >= row5Y && ty <= row5Y + 26) {
        if (tx >= 8 && tx <= 156) {
            cfg.autoCyclePages = !cfg.autoCyclePages;
            Hardware.saveSettings();
            m_pageNeedsFullRedraw = true;
        } else if (tx >= 164 && tx <= 312) {
            Hardware.resetSettings();
            Obd.toggleDemoMode(Hardware.getSettings().demoMode);
            m_pageNeedsFullRedraw = true;
        }
    }
}

// ============================================================================
// Custom Vector Drawing Primitives
// ============================================================================

void DisplayManager::drawArcBar(int16_t cx, int16_t cy, int16_t r, int16_t thickness, float startAngle, float endAngle, uint16_t color) {
    float step = 2.0f;
    for (float a = startAngle; a <= endAngle; a += step) {
        float rad = a * 0.0174532925f; // deg to rad
        float cosA = cos(rad);
        float sinA = sin(rad);

        int16_t x1 = cx + (int16_t)((r - thickness) * cosA);
        int16_t y1 = cy + (int16_t)((r - thickness) * sinA);
        int16_t x2 = cx + (int16_t)(r * cosA);
        int16_t y2 = cy + (int16_t)(r * sinA);

        m_tft.drawLine(x1, y1, x2, y2, color);
    }
}

void DisplayManager::drawCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* title) {
    m_tft.fillRoundRect(x, y, w, h, 4, COLOR_CARD_BG);
    m_tft.drawRoundRect(x, y, w, h, 4, COLOR_CARD_BORDER);
    m_tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD_BG);
    m_tft.drawCentreString(title, x + (w / 2), y + 6, 1);
}

void DisplayManager::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float percent, uint16_t fgColor, uint16_t bgColor) {
    percent = constrain(percent, 0.0f, 1.0f);
    m_tft.fillRect(x, y, w, h, bgColor);
    int fillW = (int)(w * percent);
    if (fillW > 0) {
        m_tft.fillRect(x, y, fillW, h, fgColor);
    }
    m_tft.drawRect(x, y, w, h, COLOR_CARD_BORDER);
}

void DisplayManager::drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t btnColor, uint16_t txtColor) {
    m_tft.fillRoundRect(x, y, w, h, 4, btnColor);
    m_tft.drawRoundRect(x, y, w, h, 4, COLOR_CARD_BORDER);
    m_tft.setTextColor(txtColor, btnColor);
    m_tft.drawCentreString(label, x + (w / 2), y + (h / 2) - 6, 2);
}

void DisplayManager::drawGearBadge(int16_t x, int16_t y, int gear) {
    int bw = 30;
    int bh = 24;
    m_tft.fillRoundRect(x, y, bw, bh, 3, 0x2124);
    m_tft.drawRoundRect(x, y, bw, bh, 3, COLOR_CARD_BORDER);

    char gBuf[4];
    if (gear == 0) {
        snprintf(gBuf, sizeof(gBuf), "N");
        m_tft.setTextColor(COLOR_ACCENT_AMBER, 0x2124);
    } else {
        snprintf(gBuf, sizeof(gBuf), "%d", gear);
        m_tft.setTextColor(COLOR_ACCENT_GRN, 0x2124);
    }
    m_tft.drawCentreString(gBuf, x + (bw / 2), y + 4, 2);
}
