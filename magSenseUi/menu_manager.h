#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include <display_utils.h>
#include <button_handler.h>
#include <sensor_manager.h>

class MenuManager
{
private:
    // Identificadores de opcion (no son posiciones en pantalla):
    // 0-7 ganancia, 8 serie, 9-10 vista, 11-13 rangos,
    // 14 entrada "Gain" del menu principal, 15 "< Back" del submenu de ganancia
    enum
    {
        OPT_SERIAL = 8,
        OPT_VIEW_MODERN = 9,
        OPT_VIEW_CLASSIC = 10,
        OPT_RANGE = 11, // rango vertical de la grafica (uT)
        OPT_WARN = 12,  // umbral WARNING (uT)
        OPT_ERROR = 13, // umbral MAGNETIZED / error (uT)
        OPT_GAIN_MENU = 14,
        OPT_BACK = 15
    };

    // Niveles del menu
    enum
    {
        LEVEL_MAIN = 0, // Gain > / Serial / Views / Range / Warn / Error
        LEVEL_GAIN = 1  // 8 ganancias + Back
    };

    static const int MAIN_SIZE = 7; // posiciones del menu principal
    static const int GAIN_SIZE = 9; // 8 ganancias + Back

    // Etiquetas por identificador de opcion (tambien las usa showSummary)
    static char valueLabels[3][20]; // etiquetas dinamicas de 11-13
    static char gainEntryLabel[20]; // "Gain 2X >" (entrada del menu principal)
    static const char *gainLabels[14];
    static const mlx90393_gain gainValues[8];
    static const int visibleMenuOptions;

    // Etiquetas de la pantalla actual, por posicion
    static const char *menuLabels[9];

    // Estado de navegacion
    static int level;
    static int cursor;
    static int menuScrollOffset;
    static bool isConfigured;
    static bool inMenuMode;

    // Edicion de un valor numerico (SELECT entra, UP/DOWN cambian, SELECT guarda)
    static bool editing;
    static int editValue;

    // Timing
    static unsigned long menuStartTime;
    static const unsigned long MENU_TIMEOUT;

    static int listSize();
    static int optionAt(int pos);
    static void adjustScroll();
    static void refreshLabels();
    static void drawMenu();
    static void editBounds(int option, int &lo, int &hi, int &step);
    static int currentValueOf(int option);
    static void commitEdit();
    static void applyRanges();
    static int activeOption();

public:
    static void begin();
    static void handleMenuNavigation();
    static void enterMenuMode();
    static void returnToMenu();
    static bool waitForSetup(int seconds = 3);
    static bool isInMenuMode() { return inMenuMode; }
    static int getCurrentOption() { return activeOption(); }
    static const char *const *getGainLabels() { return gainLabels; }
};

// Static constant definitions
char MenuManager::valueLabels[3][20] = {"", "", ""};
char MenuManager::gainEntryLabel[20] = "";

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

const int MenuManager::visibleMenuOptions = 5;
const unsigned long MenuManager::MENU_TIMEOUT = 5000;
const char *MenuManager::menuLabels[9] = {};

// Static variable initialization
int MenuManager::level = MenuManager::LEVEL_MAIN;
int MenuManager::cursor = 0;
int MenuManager::menuScrollOffset = 0;
bool MenuManager::isConfigured = false;
bool MenuManager::inMenuMode = false;
bool MenuManager::editing = false;
int MenuManager::editValue = 0;
unsigned long MenuManager::menuStartTime = 0;

// Implementations
void MenuManager::begin()
{
    // Vista y rangos guardados en EEPROM (SensorManager::begin() ya debe haberse ejecutado)
    DisplayUtils::setViewMode(SensorManager::getViewMode());
    applyRanges();
    refreshLabels();
}

// Numero de entradas del nivel actual
int MenuManager::listSize()
{
    return (level == LEVEL_MAIN) ? MAIN_SIZE : GAIN_SIZE;
}

// Identificador de opcion que hay en una posicion del nivel actual
int MenuManager::optionAt(int pos)
{
    if (level == LEVEL_MAIN)
    {
        static const int mainOptions[MAIN_SIZE] = {
            OPT_GAIN_MENU, OPT_SERIAL, OPT_VIEW_MODERN, OPT_VIEW_CLASSIC,
            OPT_RANGE, OPT_WARN, OPT_ERROR};
        return mainOptions[pos];
    }
    return (pos < 8) ? pos : (int)OPT_BACK;
}

// Opcion bajo el cursor
int MenuManager::activeOption()
{
    return optionAt(cursor);
}

void MenuManager::adjustScroll()
{
    if (cursor < menuScrollOffset)
    {
        menuScrollOffset = cursor;
    }
    else if (cursor >= menuScrollOffset + visibleMenuOptions)
    {
        menuScrollOffset = cursor - visibleMenuOptions + 1;
    }
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

// Reconstruye las etiquetas dinamicas y la lista de la pantalla actual.
// En edicion, el valor que se esta editando se muestra entre < >.
void MenuManager::refreshLabels()
{
    const char *names[3] = {"Range", "Warn", "Error"};
    int active = activeOption();
    for (int i = 0; i < 3; i++)
    {
        int opt = OPT_RANGE + i;
        bool edit = editing && active == opt;
        int v = edit ? editValue : currentValueOf(opt);
        if (edit)
            snprintf(valueLabels[i], sizeof(valueLabels[i]), "%s: <%d> uT", names[i], v);
        else
            snprintf(valueLabels[i], sizeof(valueLabels[i]), "%s: %d uT", names[i], v);
    }

    // Entrada del menu principal: muestra la ganancia vigente
    snprintf(gainEntryLabel, sizeof(gainEntryLabel), "%s >",
             gainLabels[SensorManager::getCurrentGainIndex()]);

    for (int pos = 0; pos < listSize(); pos++)
    {
        int id = optionAt(pos);
        if (id == OPT_GAIN_MENU)
            menuLabels[pos] = gainEntryLabel;
        else if (id == OPT_BACK)
            menuLabels[pos] = "< Back";
        else
            menuLabels[pos] = gainLabels[id];
    }
}

void MenuManager::drawMenu()
{
    refreshLabels();
    DisplayUtils::showMenu(menuLabels, listSize(), cursor, menuScrollOffset,
                           level == LEVEL_MAIN ? "MagSenseUI" : "Gain");
}

void MenuManager::commitEdit()
{
    int id = activeOption();
    if (id == OPT_RANGE)
        SensorManager::setRange(editValue);
    else if (id == OPT_WARN)
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
            editBounds(activeOption(), lo, hi, step);
            if (up)
                editValue = (editValue + step > hi) ? hi : editValue + step;
            else
                editValue = (editValue - step < lo) ? lo : editValue - step;
            buttonPressed = true;
            menuStartTime = currentTime;
        }

        if (select)
        {
            commitEdit();
            editing = false;
            buttonPressed = true;
            menuStartTime = currentTime;
        }
    }
    else
    {
        if (up)
        {
            cursor = (cursor == 0) ? listSize() - 1 : cursor - 1;
            adjustScroll();
            buttonPressed = true;
            menuStartTime = currentTime;
        }

        if (down)
        {
            cursor = (cursor + 1) % listSize();
            adjustScroll();
            buttonPressed = true;
            menuStartTime = currentTime;
        }

        if (select)
        {
            int id = activeOption();

            if (id == OPT_GAIN_MENU)
            {
                // Abre el submenu con el cursor sobre la ganancia vigente
                level = LEVEL_GAIN;
                cursor = SensorManager::getCurrentGainIndex();
                menuScrollOffset = 0;
                adjustScroll();
                buttonPressed = true;
                menuStartTime = currentTime;
            }
            else if (id == OPT_BACK)
            {
                // Vuelve al menu principal, sobre la entrada Gain
                level = LEVEL_MAIN;
                cursor = 0;
                menuScrollOffset = 0;
                buttonPressed = true;
                menuStartTime = currentTime;
            }
            else if (id >= OPT_RANGE)
            {
                // Entra en edicion del valor; no sale del menu
                editing = true;
                editValue = currentValueOf(id);
                buttonPressed = true;
                menuStartTime = currentTime;
            }
            else if (id >= OPT_VIEW_MODERN)
            {
                // Cambio de vista: no toca ganancia ni modo serie
                int newView = (id == OPT_VIEW_CLASSIC) ? DisplayUtils::VIEW_CLASSIC
                                                      : DisplayUtils::VIEW_MODERN;
                DisplayUtils::setViewMode(newView);
                SensorManager::setViewMode(newView);
                SensorManager::saveViewToEEPROM();

                isConfigured = true;
                inMenuMode = false;

                // Resumen con la etiqueta de la vista elegida
                DisplayUtils::showSummary(false, id, gainLabels);
                return;
            }
            else
            {
                if (id == OPT_SERIAL)
                {
                    SensorManager::setSerialMode(true);
                }
                else
                {
                    SensorManager::setSerialMode(false);
                    SensorManager::setGain(id);
                    SensorManager::saveGainToEEPROM();
                }

                isConfigured = true;
                inMenuMode = false;

                DisplayUtils::showSummary(SensorManager::isSerialModeActive(),
                                          id, gainLabels);
                return;
            }
        }
    }

    if (currentTime - menuStartTime > MENU_TIMEOUT)
    {
        // Al caducar no se aplica nada: una edicion sin confirmar se descarta
        editing = false;

        isConfigured = true;
        inMenuMode = false;

        // Resumen del estado real (modo serie o ganancia vigente)
        DisplayUtils::showSummary(SensorManager::isSerialModeActive(),
                                  getCurrentOption(), gainLabels);
        return;
    }

    if (buttonPressed || needsRedraw)
    {
        drawMenu();
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

// Estado de partida al abrir el menu: nivel principal, cursor arriba
void MenuManager::enterMenuMode()
{
    inMenuMode = true;
    editing = false;
    level = LEVEL_MAIN;
    cursor = 0;
    menuScrollOffset = 0;
    menuStartTime = millis();
    drawMenu();
}

void MenuManager::returnToMenu()
{
    enterMenuMode();
}

#endif
