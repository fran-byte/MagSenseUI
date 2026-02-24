#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

class ButtonHandler
{
private:
    // Button pins
    static const int BUTTON_UP;
    static const int BUTTON_DOWN;
    static const int BUTTON_SELECT;

    // Debounce variables
    static unsigned long lastDebounceTimeUp;
    static unsigned long lastDebounceTimeDown;
    static unsigned long lastDebounceTimeSelect;
    static const unsigned long debounceDelay;

    static int stableButtonUpState;
    static int stableButtonDownState;
    static int stableButtonSelectState;
    static int lastReadingUp;
    static int lastReadingDown;
    static int lastReadingSelect;
    static int lastButtonUpState;
    static int lastButtonDownState;
    static int lastButtonSelectState;

public:
    static void begin();
    static void update();
    static bool isUpPressed();
    static bool isDownPressed();
    static bool isSelectPressed();
    static bool wasUpPressed();
    static bool wasDownPressed();
    static bool wasSelectPressed();
};

// Static constant definitions
const int ButtonHandler::BUTTON_UP = 5;
const int ButtonHandler::BUTTON_DOWN = 6;
const int ButtonHandler::BUTTON_SELECT = 7;
const unsigned long ButtonHandler::debounceDelay = 50;

// Static variable initialization
unsigned long ButtonHandler::lastDebounceTimeUp = 0;
unsigned long ButtonHandler::lastDebounceTimeDown = 0;
unsigned long ButtonHandler::lastDebounceTimeSelect = 0;
int ButtonHandler::stableButtonUpState = HIGH;
int ButtonHandler::stableButtonDownState = HIGH;
int ButtonHandler::stableButtonSelectState = HIGH;
int ButtonHandler::lastReadingUp = HIGH;
int ButtonHandler::lastReadingDown = HIGH;
int ButtonHandler::lastReadingSelect = HIGH;
int ButtonHandler::lastButtonUpState = HIGH;
int ButtonHandler::lastButtonDownState = HIGH;
int ButtonHandler::lastButtonSelectState = HIGH;

// Implementations
void ButtonHandler::begin()
{
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);
}

void ButtonHandler::update()
{
    unsigned long currentTime = millis();

    // Button UP
    int readingUp = digitalRead(BUTTON_UP);
    if (readingUp != lastReadingUp)
        lastDebounceTimeUp = currentTime;
    if ((currentTime - lastDebounceTimeUp) > debounceDelay)
        stableButtonUpState = readingUp;
    lastReadingUp = readingUp;

    // Button DOWN
    int readingDown = digitalRead(BUTTON_DOWN);
    if (readingDown != lastReadingDown)
        lastDebounceTimeDown = currentTime;
    if ((currentTime - lastDebounceTimeDown) > debounceDelay)
        stableButtonDownState = readingDown;
    lastReadingDown = readingDown;

    // Button SELECT
    int readingSelect = digitalRead(BUTTON_SELECT);
    if (readingSelect != lastReadingSelect)
        lastDebounceTimeSelect = currentTime;
    if ((currentTime - lastDebounceTimeSelect) > debounceDelay)
        stableButtonSelectState = readingSelect;
    lastReadingSelect = readingSelect;
}

bool ButtonHandler::wasUpPressed()
{
    bool pressed = (stableButtonUpState == LOW && lastButtonUpState == HIGH);
    lastButtonUpState = stableButtonUpState;
    return pressed;
}

bool ButtonHandler::wasDownPressed()
{
    bool pressed = (stableButtonDownState == LOW && lastButtonDownState == HIGH);
    lastButtonDownState = stableButtonDownState;
    return pressed;
}

bool ButtonHandler::wasSelectPressed()
{
    bool pressed = (stableButtonSelectState == LOW && lastButtonSelectState == HIGH);
    lastButtonSelectState = stableButtonSelectState;
    return pressed;
}

bool ButtonHandler::isUpPressed()
{
    return stableButtonUpState == LOW;
}

bool ButtonHandler::isDownPressed()
{
    return stableButtonDownState == LOW;
}

bool ButtonHandler::isSelectPressed()
{
    return stableButtonSelectState == LOW;
}

#endif