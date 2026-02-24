#ifndef ALERT_SYMBOLS_H
#define ALERT_SYMBOLS_H

#include <U8g2lib.h>

class AlertSymbols
{
public:

    // Signo de exclamación (3x8 px)
    // Usar junto al vúmetro para WARNING (delta > 125 uT)
    //
    //  ***
    //  ***
    //  ***
    //  ***
    //  ***
    //
    //  ***  <- punto
    //
    template <typename U8G2>
    static void drawWarning(U8G2 &display, int x, int y) {
        // Línea vertical del !
        display.drawBox(x, y,     3, 5);
        // Punto del !
        display.drawBox(x, y + 6, 3, 2);
    }

    // Símbolo de radiación simplificado (11x8 px)
    // Círculo central + 3 rayos que irradian (0°, 120°, 240°)
    // Usar para MAGNETIZED (delta > 175 uT)
    //
    //  \     /
    //   \   /
    //    [o]       <- círculo central
    //   /   \
    //  /     \
    //     |
    //
    template <typename U8G2>
    static void drawRadiation(U8G2 &display, int x, int y) {
        int cx = x + 5;
        int cy = y + 4;

        // Círculo central (radio 1)
        display.drawCircle(cx, cy, 1);

        // Rayo arriba (0°)
        display.drawPixel(cx,     cy - 2);
        display.drawPixel(cx,     cy - 3);

        // Rayo abajo-izquierda (240°)
        display.drawPixel(cx - 2, cy + 1);
        display.drawPixel(cx - 3, cy + 2);

        // Rayo abajo-derecha (120°)
        display.drawPixel(cx + 2, cy + 1);
        display.drawPixel(cx + 3, cy + 2);

        // Separadores entre rayos
        display.drawPixel(cx - 2, cy - 1);
        display.drawPixel(cx - 3, cy - 2);
        display.drawPixel(cx + 2, cy - 1);
        display.drawPixel(cx + 3, cy - 2);
        display.drawPixel(cx,     cy + 2);
        display.drawPixel(cx,     cy + 3);
    }
};

#endif
