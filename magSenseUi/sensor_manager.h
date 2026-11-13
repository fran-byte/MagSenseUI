#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Adafruit_MLX90393.h>
#include <EEPROM.h>

class SensorManager
{
public:
    // Limites y valores por defecto de los parametros configurables (uT).
    // enum anonimo: evita definiciones fuera de clase (ODR) en C++11/14.
    enum
    {
        RANGE_MIN = 25,
        RANGE_MAX = 1000,
        RANGE_STEP = 25,
        RANGE_DEFAULT = 225,
        THR_MIN = 5,
        THR_MAX = 1000,
        THR_STEP = 5,
        WARN_DEFAULT = 125,
        ERROR_DEFAULT = 175,
        // Intervalo entre tomas (ms). Minimo 100: con OSR 3 y filtro 5 el
        // MLX90393 tarda ~50 ms o mas en cada conversion.
        INTERVAL_MIN = 100,
        INTERVAL_MAX = 2000,
        INTERVAL_STEP = 50,
        INTERVAL_DEFAULT = 500
    };

private:
    static Adafruit_MLX90393 magneticSensor;

    // Sensor data
    static float magX, magY, magZ;
    static float prevX, prevY, prevZ;
    static bool readSuccess;
    static bool baselineSet; // true tras la primera lectura valida

    // Timing
    static unsigned long lastMeasurementTime;
    static uint16_t measureInterval; // ms entre tomas (EEPROM 8-9)

    // Configuration
    static int currentGainIndex;
    static bool serialModeActive;
    static int viewMode; // 0 = moderna, 1 = clasica (EEPROM direccion 1)

    // Rangos (EEPROM: 2 = rango grafica, 4 = warning, 6 = error; uint16_t)
    static uint16_t graphRange;
    static uint16_t warnThreshold;
    static uint16_t errorThreshold;

public:
    static void begin();
    static void update();
    static bool readData();
    static void setGain(int gainIndex);
    static void setSerialMode(bool mode);
    static int getCurrentGainIndex() { return currentGainIndex; }
    static bool isSerialModeActive() { return serialModeActive; }
    static float getX() { return magX; }
    static float getY() { return magY; }
    static float getZ() { return magZ; }
    static float getPrevX() { return prevX; }
    static float getPrevY() { return prevY; }
    static float getPrevZ() { return prevZ; }
    static bool getReadSuccess() { return readSuccess; }
    static void saveGainToEEPROM();
    static void loadGainFromEEPROM();
    static void setViewMode(int mode) { viewMode = (mode == 1) ? 1 : 0; }
    static int getViewMode() { return viewMode; }
    static void saveViewToEEPROM();
    static void loadViewFromEEPROM();

    static int getRange() { return graphRange; }
    static int getWarning() { return warnThreshold; }
    static int getError() { return errorThreshold; }
    static void setRange(int v) { graphRange = (uint16_t)v; }
    static void setWarning(int v) { warnThreshold = (uint16_t)v; }
    static void setError(int v) { errorThreshold = (uint16_t)v; }
    static int getInterval() { return measureInterval; }
    static void setInterval(int v) { measureInterval = (uint16_t)v; }
    static void saveRangesToEEPROM();
    static void loadRangesFromEEPROM();

    // Tara: la lectura actual pasa a ser la anterior (Δ = 0)
    static void tare();
};

// Static variable initialization
Adafruit_MLX90393 SensorManager::magneticSensor;
float SensorManager::magX = 0;
float SensorManager::magY = 0;
float SensorManager::magZ = 0;
float SensorManager::prevX = 0;
float SensorManager::prevY = 0;
float SensorManager::prevZ = 0;
bool SensorManager::readSuccess = false;
bool SensorManager::baselineSet = false;
unsigned long SensorManager::lastMeasurementTime = 0;
uint16_t SensorManager::measureInterval = SensorManager::INTERVAL_DEFAULT;
int SensorManager::currentGainIndex = 0;
bool SensorManager::serialModeActive = false;
int SensorManager::viewMode = 0;
uint16_t SensorManager::graphRange = SensorManager::RANGE_DEFAULT;
uint16_t SensorManager::warnThreshold = SensorManager::WARN_DEFAULT;
uint16_t SensorManager::errorThreshold = SensorManager::ERROR_DEFAULT;

// Implementations
void SensorManager::begin()
{
    EEPROM.begin(10); // 0 ganancia, 1 vista, 2-3 rango, 4-5 warning, 6-7 error, 8-9 intervalo
    loadGainFromEEPROM();
    loadViewFromEEPROM();
    loadRangesFromEEPROM();

    if (!magneticSensor.begin_I2C())
    {
        return;
    }

    magneticSensor.setResolution(MLX90393_X, MLX90393_RES_17);
    magneticSensor.setResolution(MLX90393_Y, MLX90393_RES_17);
    magneticSensor.setResolution(MLX90393_Z, MLX90393_RES_17);
    magneticSensor.setOversampling(MLX90393_OSR_3);
    magneticSensor.setFilter(MLX90393_FILTER_5);
}

void SensorManager::update()
{
    unsigned long now = millis();

    if (now - lastMeasurementTime >= measureInterval)
    {
        lastMeasurementTime = now;

        // Save previous values BEFORE reading new ones
        prevX = magX;
        prevY = magY;
        prevZ = magZ;

        readSuccess = magneticSensor.readData(&magX, &magY, &magZ);

        // Primera lectura valida: sin muestra anterior real, asi que la
        // referencia es ella misma (delta = 0). Antes prev valia 0 y la primera
        // delta era |B| completo, disparando un MAGNETIZED falso al arrancar.
        if (readSuccess && !baselineSet)
        {
            prevX = magX;
            prevY = magY;
            prevZ = magZ;
            baselineSet = true;
        }

        if (serialModeActive && readSuccess)
        {
            Serial.print("X: ");
            Serial.print(magX, 2);
            Serial.print(" uT\t");
            Serial.print("Y: ");
            Serial.print(magY, 2);
            Serial.print(" uT\t");
            Serial.print("Z: ");
            Serial.print(magZ, 2);
            Serial.println(" uT");
        }
    }
}

bool SensorManager::readData()
{
    return readSuccess;
}

void SensorManager::setGain(int gainIndex)
{
    currentGainIndex = gainIndex;
    mlx90393_gain gainValues[] = {
        MLX90393_GAIN_1X, MLX90393_GAIN_1_33X, MLX90393_GAIN_1_67X,
        MLX90393_GAIN_2X, MLX90393_GAIN_2_5X, MLX90393_GAIN_3X,
        MLX90393_GAIN_4X, MLX90393_GAIN_5X};
    if (gainIndex < 8)
    {
        magneticSensor.setGain(gainValues[gainIndex]);
    }
}

void SensorManager::setSerialMode(bool mode)
{
    serialModeActive = mode;
}

void SensorManager::saveGainToEEPROM()
{
    EEPROM.write(0, currentGainIndex);
    EEPROM.commit();
}

void SensorManager::loadGainFromEEPROM()
{
    // Solo hay 8 ganancias (0-7); antes aceptaba 8 (< 9), que setGain ignoraba
    int savedOption = EEPROM.read(0);
    if (savedOption >= 0 && savedOption < 8)
    {
        currentGainIndex = savedOption;
    }
}

void SensorManager::saveViewToEEPROM()
{
    EEPROM.write(1, viewMode);
    EEPROM.commit();
}

void SensorManager::loadViewFromEEPROM()
{
    // Solo 1 = clasica; cualquier otro valor (0, 0xFF de EEPROM virgen) = moderna
    viewMode = (EEPROM.read(1) == 1) ? 1 : 0;
}

void SensorManager::tare()
{
    prevX = magX;
    prevY = magY;
    prevZ = magZ;
}

void SensorManager::saveRangesToEEPROM()
{
    EEPROM.put(2, graphRange);
    EEPROM.put(4, warnThreshold);
    EEPROM.put(6, errorThreshold);
    EEPROM.put(8, measureInterval);
    EEPROM.commit();
}

void SensorManager::loadRangesFromEEPROM()
{
    uint16_t r = 0, w = 0, e = 0;
    EEPROM.get(2, r);
    EEPROM.get(4, w);
    EEPROM.get(6, e);

    // EEPROM virgen (0xFFFF) o corrupta: se queda con los valores por defecto.
    // Se exige tambien warning < error.
    bool valid = (r >= RANGE_MIN && r <= RANGE_MAX) &&
                 (w >= THR_MIN && w <= THR_MAX) &&
                 (e >= THR_MIN && e <= THR_MAX) &&
                 (w < e);
    if (valid)
    {
        graphRange = r;
        warnThreshold = w;
        errorThreshold = e;
    }
    else
    {
        graphRange = RANGE_DEFAULT;
        warnThreshold = WARN_DEFAULT;
        errorThreshold = ERROR_DEFAULT;
    }

    // El intervalo se valida aparte: una EEPROM de una version anterior tiene
    // 0xFFFF aqui y los rangos siguen siendo validos.
    uint16_t iv = 0;
    EEPROM.get(8, iv);
    measureInterval = (iv >= INTERVAL_MIN && iv <= INTERVAL_MAX) ? iv : INTERVAL_DEFAULT;
}

#endif
