#ifndef DETECTOR_H
#define DETECTOR_H

#include <Arduino.h>
#include <math.h>

// Logica de deteccion, sin dependencias de pantalla ni de sensor:
//  - Δ por eje entre la muestra actual y la anterior
//  - Peak hold: el maximo de cada eje se retiene HOLD_MS antes de decaer
//  - Histeresis en los umbrales: se entra al superar el umbral y solo se sale
//    al bajar un 5% por debajo, para que el estado no parpadee
// Se actualiza en cada iteracion de loop(), tambien en modo serie.
class Detector
{
public:
    enum State
    {
        STATE_OK = 0,
        STATE_WARNING = 1,
        STATE_MAGNETIZED = 2
    };

    enum
    {
        HOLD_MS = 3000 // tiempo que se retiene el pico de Δ
    };

private:
    static float warnThr;
    static float errThr;

    static float liveDelta[3];
    static float heldDelta[3];
    static unsigned long heldTime[3];
    static bool axisWarn[3];
    static bool axisErr[3];

    static float liveMax;
    static float heldMax;
    static int heldMaxAxis;

    static bool warnLatched;
    static bool errLatched;
    static bool valid;

    static bool latch(bool current, float value, float threshold);

public:
    static void setThresholds(float warn, float error);
    static void update(float magX, float magY, float magZ,
                       float prevX, float prevY, float prevZ,
                       bool readSuccess);
    static void reset();

    static bool isValid() { return valid; }
    static State getState()
    {
        return errLatched ? STATE_MAGNETIZED : (warnLatched ? STATE_WARNING : STATE_OK);
    }

    // Δ maxima instantanea (la que se grafica)
    static float getLiveMaxDelta() { return liveMax; }

    // Δ con peak hold (la que se muestra y la que decide el estado)
    static float getMaxDelta() { return heldMax; }
    static char getMaxAxis() { return "XYZ"[heldMaxAxis]; }
    static float getDelta(int axis) { return heldDelta[axis]; }
    static bool axisWarning(int axis) { return axisWarn[axis]; }
    static bool axisMagnetized(int axis) { return axisErr[axis]; }
};

float Detector::warnThr = 125.0f;
float Detector::errThr = 175.0f;
float Detector::liveDelta[3] = {0, 0, 0};
float Detector::heldDelta[3] = {0, 0, 0};
unsigned long Detector::heldTime[3] = {0, 0, 0};
bool Detector::axisWarn[3] = {false, false, false};
bool Detector::axisErr[3] = {false, false, false};
float Detector::liveMax = 0;
float Detector::heldMax = 0;
int Detector::heldMaxAxis = 0;
bool Detector::warnLatched = false;
bool Detector::errLatched = false;
bool Detector::valid = false;

// Histeresis: activo -> se mantiene hasta bajar de umbral * (1 - 5%)
bool Detector::latch(bool current, float value, float threshold)
{
    const float HYST = 0.05f;
    if (!current)
        return value > threshold;
    return value >= threshold * (1.0f - HYST);
}

void Detector::reset()
{
    for (int i = 0; i < 3; i++)
    {
        liveDelta[i] = 0;
        heldDelta[i] = 0;
        heldTime[i] = millis();
        axisWarn[i] = false;
        axisErr[i] = false;
    }
    liveMax = 0;
    heldMax = 0;
    heldMaxAxis = 0;
    warnLatched = false;
    errLatched = false;
}

// Cambiar umbrales reinicia los estados (los latches dependian de los antiguos)
void Detector::setThresholds(float warn, float error)
{
    warnThr = warn;
    errThr = error;
    reset();
}

void Detector::update(float magX, float magY, float magZ,
                      float prevX, float prevY, float prevZ,
                      bool readSuccess)
{
    valid = readSuccess;
    if (!readSuccess)
        return;

    unsigned long now = millis();
    float d[3] = {fabsf(magX - prevX), fabsf(magY - prevY), fabsf(magZ - prevZ)};

    liveMax = d[0];
    heldMax = 0;
    heldMaxAxis = 0;

    for (int i = 0; i < 3; i++)
    {
        liveDelta[i] = d[i];
        if (d[i] > liveMax)
            liveMax = d[i];

        // Peak hold: sube al instante, baja a la lectura actual al caducar
        if (d[i] >= heldDelta[i] || (now - heldTime[i]) > (unsigned long)HOLD_MS)
        {
            heldDelta[i] = d[i];
            heldTime[i] = now;
        }

        axisWarn[i] = latch(axisWarn[i], heldDelta[i], warnThr);
        axisErr[i] = latch(axisErr[i], heldDelta[i], errThr);

        if (heldDelta[i] > heldMax)
        {
            heldMax = heldDelta[i];
            heldMaxAxis = i;
        }
    }

    warnLatched = latch(warnLatched, heldMax, warnThr);
    errLatched = latch(errLatched, heldMax, errThr);
}

#endif
