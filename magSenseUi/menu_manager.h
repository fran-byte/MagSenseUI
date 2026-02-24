#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include <display_utils.h>
#include <button_handler.h>
#include <sensor_manager.h>

class MenuManager
{
private:
    // Menu configuration
    static const char *gainLabels[9];
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

public:
    static void begin();
    static void handleMenuNavigation();
    static void enterMenuMode();
    static void returnToMenu();
    static bool isInMenuMode() { return inMenuMode; }
    static int getCurrentOption() { return currentOption; }
    static const char *const *getGainLabels() { return gainLabels; }
};

// Static constant definitions
const char *MenuManager::gainLabels[9] = {
    "Gain: 1X", "Gain: 1.33X", "Gain: 1.67X", "Gain: 2X", "Gain: 2.5X",
    "Gain: 3X", "Gain: 4X", "Gain: 5X", "Serial ON"};

const mlx90393_gain MenuManager::gainValues[8] = {
    MLX90393_GAIN_1X, MLX90393_GAIN_1_33X, MLX90393_GAIN_1_67X,
    MLX90393_GAIN_2X, MLX90393_GAIN_2_5X, MLX90393_GAIN_3X,
    MLX90393_GAIN_4X, MLX90393_GAIN_5X};

const int MenuManager::TOTAL_OPTIONS = 9;
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
        if (currentOption == 8)
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