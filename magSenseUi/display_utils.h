#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>
#include <alert_symbols.h>
#include <iman_bitmap.h>
#include <detector.h>

class DisplayUtils
{
public:
    // Vistas de lectura seleccionables desde el menu
    enum ViewMode
    {
        VIEW_MODERN = 0, // historico de Δ + diagnostico Δ
        VIEW_CLASSIC = 1 // original: barras VU por eje + ganancia
    };

private:
    enum
    {
        HIST_LEN = 21, // 21 muestras = 20 intervalos de medida (10 s con 500 ms)
        FRAME_MS = 50  // refresco maximo de la pantalla de medida (~20 fps)
    };

    // Full buffer (F): 1 KB de RAM. firstPage()/nextPage() siguen funcionando
    // en modo F, asi que el resto de funciones no necesitan cambios.
    static U8G2_SH1106_128X64_NONAME_F_HW_I2C display;
    static int viewMode;

    // Rangos configurables desde el menu (uT). Valores por defecto = antiguos
    // valores hardcodeados. Se cambian con setRanges().
    static float graphRange;     // fondo de escala vertical de la grafica de Δ
    static float warnThreshold;  // Δ > warnThreshold  -> WARNING
    static float errorThreshold; // Δ > errorThreshold -> MAGNETIZED (siempre > warn)

    // Historico de la grafica (vista moderna). Es estado de clase para poder
    // reiniciarlo con resetMeasurement() (tara).
    static float history[HIST_LEN];
    static int histHead;  // proxima posicion a escribir
    static int histCount; // muestras validas
    static float lastX, lastY, lastZ;
    static bool firstSample;

    static unsigned long toastUntil;   // aviso "TARE" visible hasta este instante
    static unsigned long lastDrawTime; // limitador de refresco

    static void drawToast();
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
    static void showSetupPrompt(int secondsLeft);
    static void showErrorMessage(const char *msg);
    static void showMenu(const char *const labels[], int totalOptions,
                         int currentOption, int scrollOffset,
                         const char *title = "MagSenseUI");
    static void showSummary(bool serialModeActive, int currentOption,
                            const char *const labels[]);
    static void showSensorData(float magX, float magY, float magZ,
                               float prevX, float prevY, float prevZ,
                               bool readSuccess, int currentOption,
                               const char *const labels[]);
    static void setViewMode(int mode);
    static int getViewMode() { return viewMode; }
    static void setRanges(int range, int warn, int error);
    static void resetMeasurement();
    static void clearHistory();
};

// Static display initialization
U8G2_SH1106_128X64_NONAME_F_HW_I2C DisplayUtils::display(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/10, /* data=*/9);
int DisplayUtils::viewMode = DisplayUtils::VIEW_MODERN;
float DisplayUtils::graphRange = 225.0f;
float DisplayUtils::warnThreshold = 125.0f;
float DisplayUtils::errorThreshold = 175.0f;
float DisplayUtils::history[DisplayUtils::HIST_LEN] = {0};
int DisplayUtils::histHead = 0;
int DisplayUtils::histCount = 0;
float DisplayUtils::lastX = 0;
float DisplayUtils::lastY = 0;
float DisplayUtils::lastZ = 0;
bool DisplayUtils::firstSample = true;
unsigned long DisplayUtils::toastUntil = 0;
unsigned long DisplayUtils::lastDrawTime = 0;

// Implementations
void DisplayUtils::begin()
{
    display.begin();
    display.enableUTF8Print();
    // Bus I2C a 400 kHz (maximo del SH1106 y del MLX90393): una pantalla
    // completa de 1 KB tarda ~4x menos que a 100 kHz.
    display.setBusClock(400000);
}

void DisplayUtils::setViewMode(int mode)
{
    viewMode = (mode == VIEW_CLASSIC) ? VIEW_CLASSIC : VIEW_MODERN;
}

// Los umbrales tambien se pasan al detector (reinicia sus estados)
void DisplayUtils::setRanges(int range, int warn, int error)
{
    graphRange = (float)range;
    warnThreshold = (float)warn;
    errorThreshold = (float)error;
    Detector::setThresholds(warnThreshold, errorThreshold);
}

// Tara: borra la grafica y muestra el aviso. El detector y el sensor se
// reinician aparte (Detector::reset(), SensorManager::tare()).
void DisplayUtils::clearHistory()
{
    histHead = 0;
    histCount = 0;
    firstSample = true;
}

void DisplayUtils::resetMeasurement()
{
    clearHistory();
    toastUntil = millis() + 1000;
}

// Aviso breve centrado, sobre cualquier vista
void DisplayUtils::drawToast()
{
    if ((long)(toastUntil - millis()) <= 0)
        return;

    display.setFont(u8g2_font_6x10_tf);
    const char *msg = "TARE";
    int w = display.getStrWidth(msg);
    int x = (128 - w) / 2;

    display.setDrawColor(0);
    display.drawBox(x - 8, 26, w + 16, 16);
    display.setDrawColor(1);
    display.drawFrame(x - 8, 26, w + 16, 16);
    display.drawStr(x, 38, msg);
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

// Pantalla de arranque: invita a entrar en configuracion y muestra la cuenta
// atras en digitos grandes (logisoso32, 32 px de alto).
void DisplayUtils::showSetupPrompt(int secondsLeft)
{
    char num[2] = {(char)('0' + secondsLeft), 0};

    display.firstPage();
    do
    {
        display.setDrawColor(1);
        display.drawFrame(0, 0, 128, 64);

        display.setFont(u8g2_font_6x10_tf);
        const char *msg = "Press SET for setup";
        display.drawStr((128 - display.getStrWidth(msg)) / 2, 14, msg);

        display.setFont(u8g2_font_logisoso32_tn);
        display.drawStr((128 - display.getStrWidth(num)) / 2, 58, num);
    } while (display.nextPage());
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
                            int scrollOffset,
                            const char *title)
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
        display.drawStr((128 - display.getStrWidth(title)) / 2, 10, title);
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

// Despachador: la firma no cambia. Limita el refresco a ~20 fps: el sensor
// da una muestra cada 500 ms, redibujar mas rapido solo ocupa CPU y el bus I2C.
void DisplayUtils::showSensorData(float magX, float magY, float magZ,
                                  float prevX, float prevY, float prevZ,
                                  bool readSuccess, int currentOption,
                                  const char *const labels[])
{
    unsigned long now = millis();
    if (now - lastDrawTime < (unsigned long)FRAME_MS)
        return;
    lastDrawTime = now;

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
// y 0-10 X / Y / Z en 3 columnas (x = 0, 43, 86); en negativo si su Δ > error
// y 13-52 grafica de la Δ maxima instantanea a todo el ancho (21 muestras),
// escala fija 0..graphRange uT, 0 en la base. Solo la curva.
// y 55-63 Δ <eje>:<valor retenido> (izq) y OK / WARNING / MAGNETIZED (der),
// en barra inversa y parpadeante si MAGNETIZED
//
// Δ de la barra, estado y alarmas por eje vienen del Detector (peak hold e
// histeresis); la grafica muestra el valor instantaneo.
// ---------------------------------------------------------------------------
void DisplayUtils::showSensorDataModern(float magX, float magY, float magZ,
                                        float prevX, float prevY, float prevZ,
                                        bool readSuccess, int currentOption,
                                        const char *const labels[])
{
    const int GRAPH_TOP = 13;
    const int GRAPH_BOTTOM = 52;
    const int GRAPH_H = GRAPH_BOTTOM - GRAPH_TOP; // 39 px

    // Escala vertical fija: 0 uT (abajo) .. graphRange uT (arriba).
    // La Δ es un valor absoluto, siempre >= 0.
    const float span = graphRange;

    // Estado del detector (peak hold + histeresis)
    Detector::State state = Detector::getState();
    bool isWarning = (state != Detector::STATE_OK);
    bool isMagnetized = (state == Detector::STATE_MAGNETIZED);
    float maxDelta = Detector::getMaxDelta();
    char maxAxis = Detector::getMaxAxis();

    // Solo se anade al historico cuando hay una muestra nueva: esta funcion
    // se llama mas rapido que el intervalo de medida.
    // Se grafica la Δ maxima instantanea real (aunque no supere warning).
    if (readSuccess && (firstSample || magX != lastX || magY != lastY || magZ != lastZ))
    {
        history[histHead] = Detector::getLiveMaxDelta();
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
    // Si la Δ de un eje supera el umbral MAGNETIZED, su letra y su valor
    // se dibujan en negativo (caja rellena + texto en color 0).
    display.setFont(u8g2_font_6x10_tf);
    auto drawAxis = [&](int x, char axis, float value, bool alarm)
    {
        snprintf(buf, sizeof(buf), "%c%+5.0f", axis, value);
        if (alarm)
        {
            display.drawBox(x > 0 ? x - 1 : 0, 0, 38, 11);
            display.setDrawColor(0);
        }
        display.drawStr(x, 9, buf);
        display.setDrawColor(1);
    };
    drawAxis(0, 'X', magX, Detector::axisMagnetized(0));
    drawAxis(43, 'Y', magY, Detector::axisMagnetized(1));
    drawAxis(86, 'Z', magZ, Detector::axisMagnetized(2));

    // ---- Grafica de Δ a todo el ancho, solo la curva ----
    int start = (histHead - histCount + HIST_LEN) % HIST_LEN;

    // Linea continua, la muestra mas reciente en x=127 (con marcador)
    int prevPx = 0;
    int prevPy = GRAPH_BOTTOM;
    for (int i = 0; i < histCount; i++)
    {
        float v = history[(start + i) % HIST_LEN];
        int py = GRAPH_BOTTOM - (int)((v / span) * GRAPH_H + 0.5f);
        if (py < GRAPH_TOP)
            py = GRAPH_TOP;
        if (py > GRAPH_BOTTOM)
            py = GRAPH_BOTTOM;
        int px = 127 - ((histCount - 1 - i) * 127) / (HIST_LEN - 1);

        if (i > 0)
        {
            display.drawLine(prevPx, prevPy, px, py);
            display.drawLine(prevPx, prevPy, px, py);
        }
        else
        {
            display.drawPixel(px, py);
            display.drawPixel(px, py);
        }

        prevPx = px;
        prevPy = py;

        if (i == histCount - 1)
            display.drawBox(px - 2, py - 1, 3, 3);
    }

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

    // Siempre el eje con mayor variacion (valor retenido), sea cual sea el estado
    display.setFont(u8g2_font_6x10_tf);
    snprintf(buf, sizeof(buf), "%c:%.0f", maxAxis, maxDelta);
    display.drawStr(12, 63, buf);

    const char *stateText = isMagnetized ? "MAGNETIZED" : (isWarning ? "WARNING" : "OK");
    int sx = 126 - display.getStrWidth(stateText);
    display.drawStr(sx, 63, stateText);

    if (isMagnetized)
        AlertSymbols::drawRadiation(display, sx - 13, 56);
    else if (isWarning)
        AlertSymbols::drawWarning(display, sx - 12, 56);

    display.setDrawColor(1);
    drawToast();
    display.sendBuffer();
}

// ---------------------------------------------------------------------------
// VISTA CLASICA (original; umbrales configurables, Δ con peak hold)
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
            // Escala lineal: 6 barras = umbral warning, cada barra = warning/6
            // (con el valor por defecto de 125 uT, ~21 uT por barra)
            auto calculateBars = [](float delta) -> int
            {
                if (delta <= 0)
                    return 0;
                int bars = (int)(delta / (warnThreshold / 6.0f));
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

            // Draws full VU meter + alert symbol (estado por eje del Detector)
            auto drawVumeterWithAlert = [&](int x, int y, int axis)
            {
                drawVumeter(x, y, calculateBars(Detector::getDelta(axis)));
                if (Detector::axisMagnetized(axis))
                {
                    AlertSymbols::drawRadiation(display, x + 32, y);
                }
                else if (Detector::axisWarning(axis))
                {
                    AlertSymbols::drawWarning(display, x + 32, y);
                }
            };

            // X axis
            display.setCursor(12, 15);
            display.print("X:");
            formatFixedWidthNumber(22, 15, magX);
            display.print(" uT");
            drawVumeterWithAlert(76, 8, 0);

            // Y axis
            display.setCursor(12, 30);
            display.print("Y:");
            formatFixedWidthNumber(22, 30, magY);
            display.print(" uT");
            drawVumeterWithAlert(76, 23, 1);

            // Z axis
            display.setCursor(12, 45);
            display.print("Z:");
            formatFixedWidthNumber(22, 45, magZ);
            display.print(" uT");
            drawVumeterWithAlert(76, 38, 2);

            // Diagnostico: Δ del eje con mayor variacion (valor retenido) a la
            // izquierda, estado a la derecha
            Detector::State state = Detector::getState();

            // Δ dibujada como triangulo 7x7 (la fuente no tiene el glifo U+0394)
            display.drawLine(15, 54, 12, 60);
            display.drawLine(15, 54, 18, 60);
            display.drawLine(12, 60, 18, 60);

            char diag[16];
            snprintf(diag, sizeof(diag), "%c:%.0f",
                     Detector::getMaxAxis(), Detector::getMaxDelta());
            display.setCursor(22, 60);
            display.print(diag);

            // Estado a la derecha (5x7: "MAGNETIZED" = 50 px, de x=68 a x=118)
            display.setFont(u8g2_font_5x7_tf);
            if (state == Detector::STATE_MAGNETIZED)
                display.drawStr(68, 60, "MAGNETIZED");
            else if (state == Detector::STATE_WARNING)
                display.drawStr(68, 60, "WARNING");
            else
                display.drawStr(68, 60, "OK");
        }
        else
        {
            display.setCursor(10, 30);
            display.print("Read error");
        }

        drawToast();
    } while (display.nextPage());
}

#endif
