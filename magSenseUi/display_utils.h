#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <U8g2lib.h>
#include <Wire.h>
#include <alert_symbols.h>

class DisplayUtils
{
private:
    static U8G2_SH1106_128X64_NONAME_1_HW_I2C display;

public:
    static void begin();
    static void showIntro();
    static void showErrorMessage(const char *msg);
    static void showMenu(const char *const labels[], int totalOptions,
                         int currentOption, int scrollOffset);
    static void showSummary(bool serialModeActive, int currentOption,
                            const char *const labels[]);
    static void showSensorData(float magX, float magY, float magZ,
                               float prevX, float prevY, float prevZ,
                               bool readSuccess, int currentOption,
                               const char *const labels[]);
};

// Inicialización del display estático
U8G2_SH1106_128X64_NONAME_1_HW_I2C DisplayUtils::display(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/10, /* data=*/9);

// Implementaciones
void DisplayUtils::begin() {
    display.begin();
    display.enableUTF8Print();
}

void DisplayUtils::showIntro() {
    display.firstPage();
    do {
        display.setFont(u8g2_font_logisoso20_tr);
        display.drawStr(4, 40, "Fran-Byte");
    } while (display.nextPage());
    delay(1000);
}

void DisplayUtils::showErrorMessage(const char *msg) {
    display.firstPage();
    do {
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawStr(5, 30, msg);
    } while (display.nextPage());
}

void DisplayUtils::showMenu(const char *const labels[], int totalOptions,
                           int currentOption, int scrollOffset) {
    const int visibleMenuOptions = 5;
    
    display.firstPage();
    do {
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawFrame(0, 0, 128, 64);
        display.drawStr(8, 10, "Select option:");
        
        for (int i = 0; i < visibleMenuOptions; i++) {
            int idx = scrollOffset + i;
            if (idx >= totalOptions) break;
            int y = 20 + i * 10;
            if (idx == currentOption) display.drawStr(0, y, ">");
            display.setCursor(10, y);
            display.print(labels[idx]);
        }
    } while (display.nextPage());
}

void DisplayUtils::showSummary(bool serialModeActive, int currentOption,
                              const char *const labels[]) {
    display.firstPage();
    do {
        display.setFont(u8g2_font_ncenB08_tr);
        display.setCursor(8, 15);
        display.print("Configuration:");
        display.setCursor(8, 30);
        if (serialModeActive) {
            display.print("Serial: ON");
            display.setCursor(8, 45);
            display.print("Display: OFF");
        } else {
            display.print(labels[currentOption]);
        }
    } while (display.nextPage());
    delay(2000);
}

void DisplayUtils::showSensorData(float magX, float magY, float magZ,
                                 float prevX, float prevY, float prevZ,
                                 bool readSuccess, int currentOption,
                                 const char *const labels[]) {
    display.firstPage();
    do {
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawFrame(0, 0, 128, 64);
        
        if (readSuccess) {
            // Escala lineal: 6 barras = 125 uT, cada barra ~21 uT
            auto calculateBars = [](float delta) -> int {
                if (delta <= 0) return 0;
                int bars = (int)(delta / 21.0f);
                if (bars > 6) bars = 6;
                return bars;
            };

            // Vúmetro con marcos vacíos de fondo (escala fija) y relleno proporcional
            auto drawVumeter = [&](int x, int y, int bars) {
                for (int i = 0; i < 6; i++) {
                    int bx = x + (i * 5);
                    display.drawFrame(bx, y, 4, 8);
                    if (i < bars) {
                        display.drawBox(bx, y, 4, 8);
                    }
                }
            };

            auto formatFixedWidthNumber = [&](int x, int y, float value) {
                char buffer[10];
                snprintf(buffer, sizeof(buffer), "%5.0f", value);
                display.setCursor(x, y);
                display.print(buffer);
            };

            // Dibuja vúmetro completo + símbolo de alerta si supera umbral
            auto drawVumeterWithAlert = [&](int x, int y, float delta, int bars) {
                drawVumeter(x, y, bars);
                if (delta > 175) {
                    AlertSymbols::drawRadiation(display, x + 32, y);
                } else if (delta > 125) {
                    AlertSymbols::drawWarning(display, x + 32, y);
                }
            };

            float deltaX = abs(magX - prevX);
            float deltaY = abs(magY - prevY);
            float deltaZ = abs(magZ - prevZ);

            int barsX = calculateBars(deltaX);
            int barsY = calculateBars(deltaY);
            int barsZ = calculateBars(deltaZ);

            // X axis
            display.setCursor(12, 15);
            display.print("X:");
            formatFixedWidthNumber(22, 15, magX);
            display.print(" uT");
            drawVumeterWithAlert(76, 8, deltaX, barsX);

            // Y axis
            display.setCursor(12, 30);
            display.print("Y:");
            formatFixedWidthNumber(22, 30, magY);
            display.print(" uT");
            drawVumeterWithAlert(76, 23, deltaY, barsY);

            // Z axis
            display.setCursor(12, 45);
            display.print("Z:");
            formatFixedWidthNumber(22, 45, magZ);
            display.print(" uT");
            drawVumeterWithAlert(76, 38, deltaZ, barsZ);

            // Gain setting
            display.setCursor(12, 60);
            display.print(labels[currentOption]);
        } else {
            display.setCursor(10, 30);
            display.print("Read error");
        }
    } while (display.nextPage());
}

#endif
