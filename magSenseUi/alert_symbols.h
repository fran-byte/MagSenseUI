#ifndef ALERT_SYMBOLS_H
#define ALERT_SYMBOLS_H

#include <U8g2lib.h>

class AlertSymbols
{
public:
    // Exclamation mark (3x8 px)
    // Displayed next to the VU meter for WARNING (delta > 125 uT)
    //
    //  ***
    //  ***
    //  ***
    //  ***
    //  ***
    //
    //  ***  <- dot
    //
    template <typename U8G2>
    static void drawWarning(U8G2 &display, int x, int y)
    {
        // Vertical line of !
        display.drawBox(x, y, 3, 5);
        // Dot of !
        display.drawBox(x, y + 6, 3, 2);
    }

    // Simplified radiation symbol (11x8 px)
    // Central circle + 3 radiating rays (0°, 120°, 240°)
    // Displayed next to the VU meter for MAGNETIZED (delta > 175 uT)
    //
    //  \     /
    //   \   /
    //    [o]       <- central circle
    //   /   \
    //  /     \
    //     |
    //
    template <typename U8G2>
    static void drawRadiation(U8G2 &display, int x, int y)
    {
        int cx = x + 5;
        int cy = y + 4;

        // Central circle (radius 1)
        display.drawCircle(cx, cy, 1);

        // Ray up (0°)
        display.drawPixel(cx, cy - 2);
        display.drawPixel(cx, cy - 3);

        // Ray bottom-left (240°)
        display.drawPixel(cx - 2, cy + 1);
        display.drawPixel(cx - 3, cy + 2);

        // Ray bottom-right (120°)
        display.drawPixel(cx + 2, cy + 1);
        display.drawPixel(cx + 3, cy + 2);

        // Ray separators
        display.drawPixel(cx - 2, cy - 1);
        display.drawPixel(cx - 3, cy - 2);
        display.drawPixel(cx + 2, cy - 1);
        display.drawPixel(cx + 3, cy - 2);
        display.drawPixel(cx, cy + 2);
        display.drawPixel(cx, cy + 3);
    }
};

#endif