#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include <display_utils.h>
#include <button_handler.h>
#include <sensor_manager.h>

class MenuManager
{
private:
    // Indices del menu: 0-7 ganancia, 8 serie, 9-10 vista, 11-13 rangos
    enum
    {
        OPT_SERIAL = 8,
        OPT_VIEW_MODERN = 9,
        OPT_VIEW_CLASSIC = 10,
        OPT_RANGE = 11, // rango vertical de la grafica (uT)
        OPT_WARN = 12,  // umbral WARNING (uT)
        OPT_ERROR = 13  // umbral MAGNETIZED / error (uT)
    };

    // Menu configuration
    static char valueLabels[3][20]; // etiquetas dinamicas de 11-13
    static const char *gainLabels[14];
    static const mlx90393_gain gainValues[8];
    static const int TOTAL_OPTIONS;
    static const int visibleMenuOptions;

    // System state
    static int currentOption;
    static int menuScrollOffset;
    static bool isConfigured;
    static bool inMenuMode;

    // Edicion de un valor numerico (SELECT entra, UP/DOWN cambian, SELECT guarda)
    static bool editing;
    static int editValue;

    // Timing
    static unsigned long menuStartTime;
    static const unsigned long MENU_TIMEOUT;

    static void adjustScroll();
    static void restoreCurrentOption();
    static void refreshValueLabels();
    static void editBounds(int option, int &lo, int &hi, int &step);
    static int currentValueOf(int option);
    static void commitEdit();
    static void applyRanges();

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
char MenuManager::valueLabels[3][20] = {"", "", ""};

const char *MenuManager::gainLabels[14] = {
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
    "View: Classic",
    valueLabels[0], // Range: 225 uT
    valueLabels[1], // Warn: 125 uT
    valueLabels[2]  // Error: 175 uT
};

const mlx90393_gain MenuManager::gainValues[8] = {
    MLX90393_GAIN_1X, MLX90393_GAIN_1_33X, MLX90393_GAIN_1_67X,
    MLX90393_GAIN_2X, MLX90393_GAIN_2_5X, MLX90393_GAIN_3X,
    MLX90393_GAIN_4X, MLX90393_GAIN_5X};

const int MenuManager::TOTAL_OPTIONS = 14;
const int MenuManager::visibleMenuOptions = 5;
const unsigned long MenuManager::MENU_TIMEOUT = 5000;

// Static variable initialization
int MenuManager::currentOption = 0;
int MenuManager::menuScrollOffset = 0;
bool MenuManager::isConfigured = false;
bool MenuManager::inMenuMode = false;
bool MenuManager::editing = false;
int MenuManager::editValue = 0;
unsigned long MenuManager::menuStartTime = 0;

// Implementations
void MenuManager::begin()
{
    currentOption = SensorManager::getCurrentGainIndex();
    adjustScroll();

    // Vista y rangos guardados en EEPROM (SensorManager::begin() ya debe haberse ejecutado)
    DisplayUtils::setViewMode(SensorManager::getViewMode());
    applyRanges();
    refreshValueLabels();
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
// (etiqueta de ganancia / modo serie). Las opciones de vista y de rangos no son
// un estado de ganancia, asi que tras usarlas se vuelve al valor real.
void MenuManager::restoreCurrentOption()
{
    currentOption = SensorManager::isSerialModeActive()
                        ? (int)OPT_SERIAL
                        : SensorManager::getCurrentGainIndex();
    adjustScroll();
}

// Pasa los rangos guardados al modulo de dibujo
void MenuManager::applyRanges()
{
    DisplayUtils::setRanges(SensorManager::getRange(),
                            SensorManager::getWarning(),
                            SensorManager::getError());
}

int MenuManager::currentValueOf(int option)
{
    if (option == OPT_RANGE)
        return SensorManager::getRange();
    if (option == OPT_WARN)
        return SensorManager::getWarning();
    return SensorManager::getError();
}

// Limites de edicion de cada valor. Restriccion: warning < error siempre.
void MenuManager::editBounds(int option, int &lo, int &hi, int &step)
{
    if (option == OPT_RANGE)
    {
        lo = SensorManager::RANGE_MIN;
        hi = SensorManager::RANGE_MAX;
        step = SensorManager::RANGE_STEP;
    }
    else if (option == OPT_WARN)
    {
        lo = SensorManager::THR_MIN;
        hi = SensorManager::getError() - SensorManager::THR_STEP;
        step = SensorManager::THR_STEP;
    }
    else // OPT_ERROR
    {
        lo = SensorManager::getWarning() + SensorManager::THR_STEP;
        hi = SensorManager::THR_MAX;
        step = SensorManager::THR_STEP;
    }
}

// Reconstruye las tres etiquetas dinamicas. En edicion, el valor que se esta
// editando se muestra entre < >.
void MenuManager::refreshValueLabels()
{
    const char *names[3] = {"Range", "Warn", "Error"};
    for (int i = 0; i < 3; i++)
    {
        int opt = OPT_RANGE + i;
        bool edit = editing && currentOption == opt;
        int v = edit ? editValue : currentValueOf(opt);
        if (edit)
            snprintf(valueLabels[i], sizeof(valueLabels[i]), "%s: <%d> uT", names[i], v);
        else
            snprintf(valueLabels[i], sizeof(valueLabels[i]), "%s: %d uT", names[i], v);
    }
}

void MenuManager::commitEdit()
{
    if (currentOption == OPT_RANGE)
        SensorManager::setRange(editValue);
    else if (currentOption == OPT_WARN)
        SensorManager::setWarning(editValue);
    else
        SensorManager::setError(editValue);

    SensorManager::saveRangesToEEPROM();
    applyRanges();
}

void MenuManager::handleMenuNavigation()
{
    static bool needsRedraw = true;
    bool buttonPressed = false;
    unsigned long currentTime = millis();

    ButtonHandler::update();

    bool up = ButtonHandler::wasUpPressed();
    bool down = ButtonHandler::wasDownPressed();
    bool select = ButtonHandler::wasSelectPressed();

    if (editing)
    {
        // UP aumenta, DOWN disminuye; satura en los limites (sin dar la vuelta)
        if (up || down)
        {
            int lo, hi, step;
            editBounds(currentOption, lo, hi, step);
            if (up)
                editValue = (editValue + step > hi) ? hi : editValue + step;
            else
                editValue = (editValue - step < lo) ? lo : editValue - step;
            refreshValueLabels();
            buttonPressed = true;
            menuStartTime = currentTime;
        }

        if (select)
        {
            commitEdit();
            editing = false;
            refreshValueLabels();
            buttonPressed = true;
            menuStartTime = currentTime;
        }
    }
    else
    {
        if (up)
        {
            currentOption = (currentOption == 0) ? TOTAL_OPTIONS - 1 : currentOption - 1;
            adjustScroll();
            buttonPressed = true;
            menuStartTime = currentTime;
        }

        if (down)
        {
            currentOption = (currentOption + 1) % TOTAL_OPTIONS;
            adjustScroll();
            buttonPressed = true;
            menuStartTime = currentTime;
        }

        if (select)
        {
            if (currentOption >= OPT_RANGE)
            {
                // Entra en edicion del valor; no sale del menu
                editing = true;
                editValue = currentValueOf(currentOption);
                refreshValueLabels();
                buttonPressed = true;
                menuStartTime = currentTime;
            }
            else if (currentOption >= OPT_VIEW_MODERN)
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
            else
            {
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
        }
    }

    if (currentTime - menuStartTime > MENU_TIMEOUT)
    {
        // Una edicion sin confirmar se descarta al caducar
        if (editing)
        {
            editing = false;
            refreshValueLabels();
        }

        // Si caduca con el cursor sobre una opcion de vista o de rango, no se aplica
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
    editing = false;
    menuStartTime = millis();
    refreshValueLabels();
    DisplayUtils::showMenu(gainLabels, TOTAL_OPTIONS,
                           currentOption, menuScrollOffset);
}

void MenuManager::returnToMenu()
{
    inMenuMode = true;
    editing = false;
    menuStartTime = millis();
    refreshValueLabels();
    DisplayUtils::showMenu(gainLabels, TOTAL_OPTIONS,
                           currentOption, menuScrollOffset);
}

#endif
