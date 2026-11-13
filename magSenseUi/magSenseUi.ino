/*
  Project: MagSenseUI
  Author: Fran-Byte
  Description: Main file for MagSenseUI project
  Pin Configuration:
    - Buttons: UP(5), DOWN(6), SET(7)
    - I2C: SDA(9), SCL(10)
    - RGB LED (WS2812): ver LED_PIN en alert_led.h

  Medicion: UP = tara (reinicia grafica, picos y estado), SET = menu
*/

#include <display_utils.h>
#include <button_handler.h>
#include <sensor_manager.h>
#include <menu_manager.h>
#include <detector.h>
#include <alert_led.h>

void setup()
{
    Serial.begin(115200);
    Wire.begin(9, 10);
    Wire.setClock(400000); // I2C rapido: pantalla y MLX90393 lo admiten

    DisplayUtils::begin();
    ButtonHandler::begin();
    SensorManager::begin();
    MenuManager::begin();
    AlertLed::off();

    DisplayUtils::showIntro();
    DisplayUtils::showMagnetic();

    // 3 s countdown: press SET to open the menu, otherwise start measuring
    // directly with the stored settings.
    if (MenuManager::waitForSetup())
    {
        MenuManager::enterMenuMode();
    }
}

void loop()
{
    if (MenuManager::isInMenuMode())
    {
        AlertLed::off();
        MenuManager::handleMenuNavigation();
    }
    else
    {
        ButtonHandler::update(); // Required to detect button presses outside the menu

        SensorManager::update();

        // Deteccion (peak hold + histeresis) y LED: tambien en modo serie
        Detector::update(
            SensorManager::getX(), SensorManager::getY(), SensorManager::getZ(),
            SensorManager::getPrevX(), SensorManager::getPrevY(), SensorManager::getPrevZ(),
            SensorManager::getReadSuccess());
        AlertLed::update(Detector::isValid(), Detector::getState());

        // Render sensor data screen (el refresco se limita dentro de DisplayUtils)
        if (!SensorManager::isSerialModeActive())
        {
            DisplayUtils::showSensorData(
                SensorManager::getX(), SensorManager::getY(), SensorManager::getZ(),
                SensorManager::getPrevX(), SensorManager::getPrevY(), SensorManager::getPrevZ(),
                SensorManager::getReadSuccess(),
                SensorManager::getCurrentGainIndex(),
                MenuManager::getGainLabels());
        }

        // UP = tara: mide desde cero (Δ, grafica, picos y estado)
        if (ButtonHandler::wasUpPressed())
        {
            SensorManager::tare();
            Detector::reset();
            DisplayUtils::resetMeasurement();
        }

        if (ButtonHandler::wasSelectPressed())
        {
            MenuManager::returnToMenu();
        }
    }
}
