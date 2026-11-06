#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>
#include <alert_symbols.h>
#include <iman_bitmap.h>

class DisplayUtils
{
public:
    // Vistas de lectura seleccionables desde el menu
    enum ViewMode
    {
        VIEW_MODERN = 0, // historico + |B| + diagnostico Δ
        VIEW_CLASSIC = 1 // original: barras VU por eje + ganancia
    };

private:
    // Full buffer (F): 1 KB de RAM. firstPage()/nextPage() siguen funcionando
    // en modo F, asi que el resto de funciones no necesitan cambios.
    static U8G2_SH1106_128X64_NONAME_F_HW_I2C display;
    static int viewMode;

    static void showSensorDataModern(float magX, float magY, float magZ,
                                     float prevX, float prevY, float prevZ,
                                     bool readSuccess, int currentOption,
                                     const char *const labels[]);
    static void showSensorDataClassic(float magX, float magY, float magZ,
                                      float prevX, float prevY, float prevZ,
                                      bool readSuccess, int currentOption,
                                      const char *const labels[]);

public:
    static void begin();
    static void showIntro();
    static void showMagnetic();
    static void showErrorMessage(const char *msg);
    static void showMenu(const char *const labels[], int totalOptions,
                         int currentOption, int scrollOffset);
    static void showSummary(bool serialModeActive, int currentOption,
                            const char *const labels[]);
    static void showSensorData(float magX, float magY, float magZ,
                               float prevX, float prevY, float prevZ,
                               bool readSuccess, int currentOption,
                               const char *const labels[]);
    static void setViewMode(int mode);
    static int getViewMode() { return viewMode; }
};

// Static display initialization
U8G2_SH1106_128X64_NONAME_F_HW_I2C DisplayUtils::display(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/10, /* data=*/9);
int DisplayUtils::viewMode = DisplayUtils::VIEW_MODERN;

// Implementations
void DisplayUtils::begin()
{
    display.begin();
    display.enableUTF8Print();
}

void DisplayUtils::setViewMode(int mode)
{
    viewMode = (mode == VIEW_CLASSIC) ? VIEW_CLASSIC : VIEW_MODERN;
}

void DisplayUtils::showIntro()
{
    display.firstPage();
    do
    {
        display.setFont(u8g2_font_logisoso20_tr);
        display.drawStr(4, 40, "Fran-Byte");
    } while (display.nextPage());
    delay(1000);
}

void DisplayUtils::showMagnetic()
{
    display.firstPage();
    do
    {
        display.drawXBMP(
            32, 0,
            IMAN128F_WIDTH,
            IMAN128F_HEIGHT,
            imanAscii128x64_fixed);
    } while (display.nextPage());

    delay(3000);
}

void DisplayUtils::showErrorMessage(const char *msg)
{
    display.firstPage();
    do
    {
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawStr(5, 30, msg);
    } while (display.nextPage());
}

void DisplayUtils::showMenu(const char *const labels[],
                            int totalOptions,
                            int currentOption,
                            int scrollOffset)
{
    const int visibleMenuOptions = 5;

    display.firstPage();
    do
    {
        display.setFont(u8g2_font_ncenB08_tr);

        // Marco exterior
        display.drawFrame(0, 0, 128, 64);

        // Cabecera invertida
        display.drawBox(1, 1, 126, 12);
        display.setDrawColor(0);
        display.drawStr(28, 10, "MagSenseUI");
        display.setDrawColor(1);

        // Opciones
        for (int i = 0; i < visibleMenuOptions; i++)
        {
            int idx = scrollOffset + i;

            if (idx >= totalOptions)
                break;

            int y = 22 + (i * 10);

            if (idx == currentOption)
            {
                // Línea seleccionada en negativo
                display.drawBox(2, y - 8, 124, 10);

                display.setDrawColor(0);

                display.setCursor(6, y);
                display.print("> ");
                display.print(labels[idx]);

                display.setDrawColor(1);
            }
            else
            {
                display.setCursor(6, y);
                display.print(labels[idx]);
            }
        }

    } while (display.nextPage());
}

void DisplayUtils::showSummary(bool serialModeActive, int currentOption,
                               const char *const labels[])
{
    display.firstPage();
    do
    {
        display.setFont(u8g2_font_ncenB08_tr);
        display.setCursor(8, 15);
        display.print("Configuration:");
        display.setCursor(8, 30);
        if (serialModeActive)
        {
            display.print("Serial: ON");
            display.setCursor(8, 45);
            display.print("Display: OFF");
        }
        else
        {
            display.print(labels[currentOption]);
        }
    } while (display.nextPage());
    delay(2000);
}

// Despachador: la firma no cambia, asi que el .ino no necesita modificaciones.
void DisplayUtils::showSensorData(float magX, float magY, float magZ,
                                  float prevX, float prevY, float prevZ,
                                  bool readSuccess, int currentOption,
                                  const char *const labels[])
{
    if (viewMode == VIEW_CLASSIC)
        showSensorDataClassic(magX, magY, magZ, prevX, prevY, prevZ,
                              readSuccess, currentOption, labels);
    else
        showSensorDataModern(magX, magY, magZ, prevX, prevY, prevZ,
                             readSuccess, currentOption, labels);
}

// ---------------------------------------------------------------------------
// VISTA MODERNA
//
// Layout (128x64):
// y 1- 9 X / Y / Z en 3 columnas (x = 0, 43, 86)
// y 13-44 grafica de |B| a todo el ancho (32 muestras = 16 s), zoom automatico
// y 46-53 |B| (izq) y rango vertical de la grafica (der)
// y 55-63 Δ <eje>:<valor> (izq) y OK / WARNING / MAGNETIZED (der),
// en barra inversa y parpadeante si MAGNETIZED
// ---------------------------------------------------------------------------
void DisplayUtils::showSensorDataModern(float magX, float magY, float magZ,
                                        float prevX, float prevY, float prevZ,
                                        bool readSuccess, int currentOption,
                                        const char *const labels[])
{
    // Umbrales (sin cambios)
    const float WARNING_THRESHOLD = 125.0f;
    const float MAGNETIZED_THRESHOLD = 175.0f;

    // Historico de |B|: 32 muestras x 500 ms = 16 s, cada muestra ocupa 4 columnas
    const int HIST_LEN = 32;
    const int COL_PER_SAMPLE = 4; // 32 x 4 = 128 px de ancho
    const int GRAPH_TOP = 13;
    const int GRAPH_BOTTOM = 44;
    const int GRAPH_H = GRAPH_BOTTOM - GRAPH_TOP; // 31 px
    const float MIN_SPAN = 20.0f; // uT: rango vertical minimo
    static float history[HIST_LEN];
    static int histHead = 0; // proxima posicion a escribir
    static int histCount = 0; // muestras validas
    static float lastX = 0, lastY = 0, lastZ = 0;
    static bool firstSample = true;

    // Deltas (misma formula que antes)
    float deltaX = fabsf(magX - prevX);
    float deltaY = fabsf(magY - prevY);
    float deltaZ = fabsf(magZ - prevZ);

    // Eje con mayor delta
    float maxDelta = deltaX;
    char maxAxis = 'X';
    if (deltaY > maxDelta)
    {
        maxDelta = deltaY;
        maxAxis = 'Y';
    }
    if (deltaZ > maxDelta)
    {
        maxDelta = deltaZ;
        maxAxis = 'Z';
    }

    bool isWarning = (maxDelta > WARNING_THRESHOLD);
    bool isMagnetized = (maxDelta > MAGNETIZED_THRESHOLD);

    float magnitude = sqrtf(magX * magX + magY * magY + magZ * magZ);

    // Solo se anade al historico cuando hay una muestra nueva: esta funcion
    // puede llamarse mas rapido que el intervalo de medida (500 ms).
    if (readSuccess && (firstSample || magX != lastX || magY != lastY || magZ != lastZ))
    {
        history[histHead] = magnitude;
        histHead = (histHead + 1) % HIST_LEN;
        if (histCount < HIST_LEN)
            histCount++;
        lastX = magX;
        lastY = magY;
        lastZ = magZ;
        firstSample = false;
    }

    display.clearBuffer();
    display.setDrawColor(1);

    if (!readSuccess)
    {
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(34, 36, "Read error");
        display.sendBuffer();
        return;
    }

    char buf[24];

    // ---- X / Y / Z en columnas fijas ----
    display.setFont(u8g2_font_6x10_tf);
    snprintf(buf, sizeof(buf), "X%+5.0f", magX);
    display.drawStr(0, 9, buf);
    snprintf(buf, sizeof(buf), "Y%+5.0f", magY);
    display.drawStr(43, 9, buf);
    snprintf(buf, sizeof(buf), "Z%+5.0f", magZ);
    display.drawStr(86, 9, buf);

    // ---- Grafica de |B| a todo el ancho ----
    // Rango vertical automatico: zoom a min..max con un span minimo, para que
    // las subidas y bajadas ocupen toda la altura de la grafica.
    int start = (histHead - histCount + HIST_LEN) % HIST_LEN;
    float vmin = history[start];
    float vmax = history[start];
    for (int i = 1; i < histCount; i++)
    {
        float v = history[(start + i) % HIST_LEN];
        if (v < vmin)
            vmin = v;
        if (v > vmax)
            vmax = v;
    }
    float span = vmax - vmin;
    if (span < MIN_SPAN)
    {
        float mid = (vmax + vmin) / 2.0f;
        vmin = mid - MIN_SPAN / 2.0f;
        span = MIN_SPAN;
    }
    if (vmin < 0.0f)
        vmin = 0.0f;

    // Referencias punteadas: techo y suelo del rango
    for (int x = 0; x < 128; x += 4)
    {
        display.drawPixel(x, GRAPH_TOP);
        display.drawPixel(x, GRAPH_BOTTOM);
    }

    // Linea continua, la muestra mas reciente en x=127 (con marcador)
    int x0 = 127 - (histCount - 1) * COL_PER_SAMPLE;
    int prevPy = GRAPH_BOTTOM;
    for (int i = 0; i < histCount; i++)
    {
        float v = history[(start + i) % HIST_LEN];
        int py = GRAPH_BOTTOM - (int)(((v - vmin) / span) * GRAPH_H + 0.5f);
        if (py < GRAPH_TOP)
            py = GRAPH_TOP;
        if (py > GRAPH_BOTTOM)
            py = GRAPH_BOTTOM;

        int px = x0 + i * COL_PER_SAMPLE;
        if (i > 0)
            display.drawLine(px - COL_PER_SAMPLE, prevPy, px, py);
        else
            display.drawPixel(px, py);
        prevPy = py;

        if (i == histCount - 1)
            display.drawBox(px - 2, py - 1, 3, 3);
    }

    // ---- |B| (izq) y rango vertical de la grafica (der), misma unidad ----
    bool useMilli = (magnitude >= 1000.0f);
    display.setFont(u8g2_font_6x10_tf);
    if (useMilli)
        snprintf(buf, sizeof(buf), "|B| %.2f mT", magnitude / 1000.0f);
    else
        snprintf(buf, sizeof(buf), "|B| %.0f uT", magnitude);
    display.drawStr(0, 53, buf);

    display.setFont(u8g2_font_5x7_tf);
    if (useMilli)
        snprintf(buf, sizeof(buf), "%.2f..%.2f", vmin / 1000.0f, (vmin + span) / 1000.0f);
    else
        snprintf(buf, sizeof(buf), "%.0f..%.0f", vmin, vmin + span);
    display.drawStr(126 - display.getStrWidth(buf), 53, buf);

    // ---- Barra inferior: Δ a la izquierda, estado a la derecha ----
    bool invert = isMagnetized && (((millis() / 500) % 2) == 0);
    if (invert)
    {
        display.drawBox(0, 55, 128, 9);
        display.setDrawColor(0);
    }

    // Δ dibujada como triangulo 7x7: la fuente no tiene el glifo U+0394
    display.drawLine(5, 57, 2, 63);
    display.drawLine(5, 57, 8, 63);
    display.drawLine(2, 63, 8, 63);

    display.setFont(u8g2_font_6x10_tf);
    if (isWarning)
        snprintf(buf, sizeof(buf), "%c:%.0f", maxAxis, maxDelta);
    else
        snprintf(buf, sizeof(buf), "--:0");
    display.drawStr(12, 63, buf);

    const char *state = isMagnetized ? "MAGNETIZED" : (isWarning ? "WARNING" : "OK");
    int sx = 126 - display.getStrWidth(state);
    display.drawStr(sx, 63, state);
    if (isMagnetized)
        AlertSymbols::drawRadiation(display, sx - 13, 56);
    else if (isWarning)
        AlertSymbols::drawWarning(display, sx - 12, 56);

    display.setDrawColor(1);
    display.sendBuffer();
}

// ---------------------------------------------------------------------------
// VISTA CLASICA (original, sin cambios)
// ---------------------------------------------------------------------------
void DisplayUtils::showSensorDataClassic(float magX, float magY, float magZ,
                                         float prevX, float prevY, float prevZ,
                                         bool readSuccess, int currentOption,
                                         const char *const labels[])
{
    display.firstPage();
    do
    {
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawFrame(0, 0, 128, 64);

        if (readSuccess)
        {
            // Linear scale: 6 bars = 125 uT, each bar ~21 uT
            auto calculateBars = [](float delta) -> int
            {
                if (delta <= 0)
                    return 0;
                int bars = (int)(delta / 21.0f);
                if (bars > 6)
                    bars = 6;
                return bars;
            };

            // VU meter with empty frames as background scale and proportional fill
            auto drawVumeter = [&](int x, int y, int bars)
            {
                for (int i = 0; i < 6; i++)
                {
                    int bx = x + (i * 5);
                    display.drawFrame(bx, y, 5, 10);
                    if (i < bars)
                    {
                        display.drawBox(bx, y, 5, 10);
                    }
                }
            };

            auto formatFixedWidthNumber = [&](int x, int y, float value)
            {
                char buffer[10];
                snprintf(buffer, sizeof(buffer), "%5.0f", value);
                display.setCursor(x, y);
                display.print(buffer);
            };

            // Draws full VU meter + alert symbol if threshold is exceeded
            auto drawVumeterWithAlert = [&](int x, int y, float delta, int bars)
            {
                drawVumeter(x, y, bars);
                if (delta > 175)
                {
                    AlertSymbols::drawRadiation(display, x + 32, y);
                }
                else if (delta > 125)
                {
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

            // Diagnostico (sustituye a la ganancia):
            // Δ del eje con mayor variacion a la izquierda, estado a la derecha
            float maxDelta = deltaX;
            char maxAxis = 'X';
            if (deltaY > maxDelta)
            {
                maxDelta = deltaY;
                maxAxis = 'Y';
            }
            if (deltaZ > maxDelta)
            {
                maxDelta = deltaZ;
                maxAxis = 'Z';
            }

            bool isWarning = (maxDelta > 125.0f);
            bool isMagnetized = (maxDelta > 175.0f);

            // Δ dibujada como triangulo 7x7 (la fuente no tiene el glifo U+0394)
            display.drawLine(15, 54, 12, 60);
            display.drawLine(15, 54, 18, 60);
            display.drawLine(12, 60, 18, 60);

            char diag[16];
            if (isWarning)
                snprintf(diag, sizeof(diag), "%c:%.0f", maxAxis, maxDelta);
            else
                snprintf(diag, sizeof(diag), "--:0");
            display.setCursor(22, 60);
            display.print(diag);

            // Estado a la derecha (5x7: "MAGNETIZED" = 50 px, de x=68 a x=118)
            display.setFont(u8g2_font_5x7_tf);
            if (isMagnetized)
                display.drawStr(68, 60, "MAGNETIZED");
            else if (isWarning)
                display.drawStr(68, 60, "WARNING");
            else
                display.drawStr(68, 60, "OK");
        }
        else
        {
            display.setCursor(10, 30);
            display.print("Read error");
        }
    } while (display.nextPage());
}

#endif
