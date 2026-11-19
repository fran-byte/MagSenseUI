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

// Unica salida serie del proyecto: "Serial" por el USB-C (USB CDC nativo).
// Con USB CDC On Boot desactivado, "Serial" saldria por los pines UART0 y el
// monitor serie del IDE no veria nada, asi que no se deja compilar asi.
#if (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)) && \
    !(defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1))
#error "Activar Herramientas > USB CDC On Boot > Enabled (la salida serie va por el USB-C)"
#endif

void setup()
{
    Serial.begin(115200); // USB-C
    Wire.begin(9, 10);
    Wire.setClock(I2C_CLOCK_HZ); // ver display_utils.h (400 kHz por defecto)

    DisplayUtils::begin();
    ButtonHandler::begin();
    SensorManager::begin();
    MenuManager::begin();
    AlertLed::off();

    DisplayUtils::showIntro(); // nombre + iman en una sola pantalla

    // 4 s de ojos animados: pulsar SET abre el menu, si no, empieza a medir
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
