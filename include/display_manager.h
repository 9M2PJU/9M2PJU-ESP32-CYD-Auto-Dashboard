#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "obd_data.h"
#include "hardware_manager.h"
#include "performance_tracker.h"

// Color Palette
#define COLOR_BG          0x0842   // Very dark navy / carbon black
#define COLOR_CARD_BG     0x18E3   // Dark slate card
#define COLOR_CARD_BORDER 0x31A6   // Subtle card border
#define COLOR_ACCENT_CYAN 0x07FF   // Cyan
#define COLOR_ACCENT_BLUE 0x04FF   // Bright Blue
#define COLOR_ACCENT_RED  0xF800   // Red
#define COLOR_ACCENT_AMBER 0xFDA0  // Amber / Orange
#define COLOR_ACCENT_GRN  0x07E0   // Green
#define COLOR_TEXT_DIM    0x8410   // Dim gray text
#define COLOR_TEXT_BRIGHT 0xFFFF   // Bright white text

class DisplayManager {
public:
    DisplayManager();
    void begin();
    void update(const OBDMetrics& metrics, const PerformanceTracker& perf, const TouchEvent& touch);

    void setPage(DashboardPage page);
    DashboardPage getPage() const { return m_currentPage; }
    void nextPage();
    void prevPage();

private:
    TFT_eSPI m_tft;
    DashboardPage m_currentPage = PAGE_SPORT_CLUSTER;
    DashboardPage m_lastRenderedPage = (DashboardPage)-1;
    unsigned long m_lastPageChangeMs = 0;
    bool m_pageNeedsFullRedraw = true;

    // Modal Confirmation Dialog State (for Clear DTCs)
    bool m_showConfirmModal = false;

    // Rendering methods
    void renderTopBar(const OBDMetrics& metrics);
    void renderNavBar();

    // Pages
    void renderSportCluster(const OBDMetrics& metrics);
    void renderTelemetryGrid(const OBDMetrics& metrics);
    void renderPerformance(const OBDMetrics& metrics, const PerformanceTracker& perf);
    void renderDiagnostics(const OBDMetrics& metrics);
    void renderSettings(const OBDMetrics& metrics);

    // Dialogs & Modals
    void renderConfirmModal();

    // Touch handlers per page
    void handleTouch(const TouchEvent& touch, const OBDMetrics& metrics);
    void handleTouchSport(const TouchEvent& touch);
    void handleTouchGrid(const TouchEvent& touch);
    void handleTouchPerformance(const TouchEvent& touch);
    void handleTouchDiagnostics(const TouchEvent& touch);
    void handleTouchSettings(const TouchEvent& touch);

    // Custom UI Drawing Primitives
    void drawArcBar(int16_t cx, int16_t cy, int16_t r, int16_t thickness, float startAngle, float endAngle, uint16_t color);
    void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* title);
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float percent, uint16_t fgColor, uint16_t bgColor);
    void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t btnColor, uint16_t txtColor);
    void drawGearBadge(int16_t x, int16_t y, int gear);
};

extern DisplayManager Display;
