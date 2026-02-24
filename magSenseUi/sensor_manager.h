#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Adafruit_MLX90393.h>
#include <EEPROM.h>

class SensorManager
{
private:
    static Adafruit_MLX90393 magneticSensor;

    // Sensor data
    static float magX, magY, magZ;
    static float prevX, prevY, prevZ;
    static bool readSuccess;

    // Timing
    static unsigned long lastMeasurementTime;
    static const unsigned long MEASUREMENT_INTERVAL;

    // Configuration
    static int currentGainIndex;
    static bool serialModeActive;

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
unsigned long SensorManager::lastMeasurementTime = 0;
const unsigned long SensorManager::MEASUREMENT_INTERVAL = 500;
int SensorManager::currentGainIndex = 0;
bool SensorManager::serialModeActive = false;

// Implementations
void SensorManager::begin()
{
    EEPROM.begin(1);
    loadGainFromEEPROM();

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

    if (now - lastMeasurementTime >= MEASUREMENT_INTERVAL)
    {
        lastMeasurementTime = now;

        // Save previous values BEFORE reading new ones
        prevX = magX;
        prevY = magY;
        prevZ = magZ;

        readSuccess = magneticSensor.readData(&magX, &magY, &magZ);

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
    int savedOption = EEPROM.read(0);
    if (savedOption >= 0 && savedOption < 9)
    {
        currentGainIndex = savedOption;
    }
}

#endif