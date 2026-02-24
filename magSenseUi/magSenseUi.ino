/*
  Project: MagSenseUI
  Author: Fran-Byte
  Description: Main file for MagSenseUI project
  Pin Configuration:
    - Buttons: UP(5), DOWN(6), SET(7)
    - I2C: SDA(9), SCL(10)
*/

#include <display_utils.h>
#include <button_handler.h>
#include <sensor_manager.h>
#include <menu_manager.h>

void setup()
{
    Serial.begin(115200);
    Wire.begin(9, 10);

    DisplayUtils::begin();
    ButtonHandler::begin();
    SensorManager::begin();
    MenuManager::begin();

    DisplayUtils::showIntro();
    MenuManager::enterMenuMode();
}

void loop()
{
    if (MenuManager::isInMenuMode())
    {
        MenuManager::handleMenuNavigation();
    }
    else
    {
        ButtonHandler::update(); // NECESARIO para detectar pulsaciones fuera del menú

        SensorManager::update();

        // Dibujar pantalla de datos del sensor en cada iteración
        if (!SensorManager::isSerialModeActive())
        {
            DisplayUtils::showSensorData(
                SensorManager::getX(),     SensorManager::getY(),     SensorManager::getZ(),
                SensorManager::getPrevX(), SensorManager::getPrevY(), SensorManager::getPrevZ(),
                SensorManager::getReadSuccess(),
                SensorManager::getCurrentGainIndex(),
                MenuManager::getGainLabels()
            );
        }

        if (ButtonHandler::wasSelectPressed())
        {
            MenuManager::returnToMenu();
        }
    }
}
