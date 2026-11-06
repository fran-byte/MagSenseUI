#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include <display_utils.h>
#include <button_handler.h>
#include <sensor_manager.h>

class MenuManager
{
private:
    // Indices del menu: 0-7 ganancia, 8 serie, 9-10 vista
    enum
    {
        OPT_SERIAL = 8,
        OPT_VIEW_MODERN = 9,
        OPT_VIEW_CLASSIC = 10
    };

    // Menu configuration
    static const char *gainLabels[11];
    static const mlx90393_gain gainValues[8];
    static const int TOTAL_OPTIONS;
    static const int visibleMenuOptions;

    // System state
    static int currentOption;
    static int menuScrollOffset;
    static bool isConfigured;
    static bool inMenuMode;

    // Timing
    static unsigned long menuStartTime;
    static const unsigned long MENU_TIMEOUT;

    static void adjustScroll();
    static void restoreCurrentOption();

public:
    static void begin();
    static void handleMenuNavigation();
    static void enterMenuMode();
    static void returnToMenu();
    static bool waitForSetup(int seconds = 3);
    static bool isInMenuMode() { return inMenuMode; }
    static int getCurrentOption() { return currentOption; }
    static const char *const *getGainLabels() { return gainLabels; }
};

// Static constant definitions
const char *MenuManager::gainLabels[11] = {
    "Gain 1X",
    "Gain 1.33X",
    "Gain 1.67X",
    "Gain 2X",
    "Gain 2.5X",
    "Gain 3X",
    "Gain 4X",
    "Gain 5X",
    "Serial ON",
    "View: Modern",
    "View: Classic"
};

const mlx90393_gain MenuManager::gainValues[8] = {
    MLX90393_GAIN_1X, MLX90393_GAIN_1_33X, MLX90393_GAIN_1_67X,
    MLX90393_GAIN_2X, MLX90393_GAIN_2_5X, MLX90393_GAIN_3X,
    MLX90393_GAIN_4X, MLX90393_GAIN_5X};

const int MenuManager::TOTAL_OPTIONS = 11;
const int MenuManager::visibleMenuOptions = 5;
const unsigned long MenuManager::MENU_TIMEOUT = 5000;

// Static variable initialization
int MenuManager::currentOption = 0;
int MenuManager::menuScrollOffset = 0;
bool MenuManager::isConfigured = false;
bool MenuManager::inMenuMode = false;
unsigned long MenuManager::menuStartTime = 0;

// Implementations
void MenuManager::begin()
{
    currentOption = SensorManager::getCurrentGainIndex();
    adjustScroll();

    // Vista guardada en EEPROM (SensorManager::begin() ya debe haberse ejecutado)
    DisplayUtils::setViewMode(SensorManager::getViewMode());
}

void MenuManager::adjustScroll()
{
    if (currentOption < menuScrollOffset)
    {
        menuScrollOffset = currentOption;
    }
    else if (currentOption >= menuScrollOffset + visibleMenuOptions)
    {
        menuScrollOffset = currentOption - visibleMenuOptions + 1;
    }
}

// currentOption describe el estado real que consume el resto del programa
// (etiqueta de ganancia / modo serie). Las opciones de vista no son un estado
// de ganancia, asi que tras usarlas se vuelve al valor real.
void MenuManager::restoreCurrentOption()
{
    currentOption = SensorManager::isSerialModeActive()
                        ? (int)OPT_SERIAL
                        : SensorManager::getCurrentGainIndex();
    adjustScroll();
}

void MenuManager::handleMenuNavigation()
{
    static bool needsRedraw = true;
    bool buttonPressed = false;
    unsigned long currentTime = millis();

    ButtonHandler::update();

    if (ButtonHandler::wasUpPressed())
    {
        currentOption = (currentOption == 0) ? TOTAL_OPTIONS - 1 : currentOption - 1;
        adjustScroll();
        buttonPressed = true;
        menuStartTime = currentTime;
    }

    if (ButtonHandler::wasDownPressed())
    {
        currentOption = (currentOption + 1) % TOTAL_OPTIONS;
        adjustScroll();
        buttonPressed = true;
        menuStartTime = currentTime;
    }

    if (ButtonHandler::wasSelectPressed())
    {
        if (currentOption >= OPT_VIEW_MODERN)
        {
            // Cambio de vista: no toca ganancia ni modo serie
            int newView = (currentOption == OPT_VIEW_CLASSIC) ? DisplayUtils::VIEW_CLASSIC
                                                             : DisplayUtils::VIEW_MODERN;
            DisplayUtils::setViewMode(newView);
            SensorManager::setViewMode(newView);
            SensorManager::saveViewToEEPROM();

            isConfigured = true;
            inMenuMode = false;

            // Resumen con la etiqueta de la vista elegida
            DisplayUtils::showSummary(false, currentOption, gainLabels);
            restoreCurrentOption();
            return;
        }

        if (currentOption == OPT_SERIAL)
        {
            SensorManager::setSerialMode(true);
        }
        else
        {
            SensorManager::setSerialMode(false);
            SensorManager::setGain(currentOption);
            SensorManager::saveGainToEEPROM();
        }

        isConfigured = true;
        inMenuMode = false;

        DisplayUtils::showSummary(SensorManager::isSerialModeActive(),
                                  currentOption, gainLabels);
        return;
    }

    if (currentTime - menuStartTime > MENU_TIMEOUT)
    {
        // Si caduca con el cursor sobre una opcion de vista, no se aplica
        if (currentOption >= OPT_VIEW_MODERN)
            restoreCurrentOption();

        isConfigured = true;
        inMenuMode = false;

        DisplayUtils::showSummary(SensorManager::isSerialModeActive(),
                                  currentOption, gainLabels);
        return;
    }

    if (buttonPressed || needsRedraw)
    {
        DisplayUtils::showMenu(gainLabels, TOTAL_OPTIONS,
                               currentOption, menuScrollOffset);
        needsRedraw = false;
    }
}

// Cuenta atras de arranque. Devuelve true si se pulsa SET (entrar en
// configuracion); false si termina la cuenta y se sigue con el valor guardado.
bool MenuManager::waitForSetup(int seconds)
{
    for (int s = seconds; s > 0; s--)
    {
        DisplayUtils::showSetupPrompt(s);

        unsigned long start = millis();
        while (millis() - start < 1000)
        {
            ButtonHandler::update();
            if (ButtonHandler::wasSelectPressed())
                return true;
            delay(5);
        }
    }
    return false;
}

void MenuManager::enterMenuMode()
{
    inMenuMode = true;
    menuStartTime = millis();
    DisplayUtils::showMenu(gainLabels, TOTAL_OPTIONS,
                           currentOption, menuScrollOffset);
}

void MenuManager::returnToMenu()
{
    inMenuMode = true;
    menuStartTime = millis();
    DisplayUtils::showMenu(gainLabels, TOTAL_OPTIONS,
                           currentOption, menuScrollOffset);
}

#endif
