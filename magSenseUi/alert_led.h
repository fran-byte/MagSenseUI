#ifndef ALERT_LED_H
#define ALERT_LED_H

#include <Arduino.h>
#include <detector.h>

// LED RGB WS2812 de la placa como indicador de estado, visible sin mirar la
// pantalla: verde = OK, ambar = WARNING, rojo parpadeante = MAGNETIZED
// (parpadeo sincronizado con la barra inversa de la pantalla).
//
// Pin del LED: en la ESP32-S3 Super Mini suele ser GPIO48 (algunas variantes
// usan GPIO21). VERIFICAR en tu placa y ajustar LED_PIN.
class AlertLed
{
public:
    enum
    {
        LED_PIN = 48,
        BRIGHTNESS = 24 // 0-255; bajo a proposito, el WS2812 deslumbra
    };

private:
    static uint32_t lastColor; // ultimo color escrito, para no reescribir

    static void write(uint8_t r, uint8_t g, uint8_t b);

public:
    static void off();
    static void update(bool valid, Detector::State state);
};

uint32_t AlertLed::lastColor = 0xFFFFFFFF; // valor imposible: fuerza 1a escritura

void AlertLed::write(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    if (color == lastColor)
        return;
    lastColor = color;
    neopixelWrite(LED_PIN, r, g, b);
}

void AlertLed::off()
{
    write(0, 0, 0);
}

void AlertLed::update(bool valid, Detector::State state)
{
    if (!valid)
    {
        off();
        return;
    }

    switch (state)
    {
    case Detector::STATE_MAGNETIZED:
        if (((millis() / 500) % 2) == 0)
            write(BRIGHTNESS, 0, 0);
        else
            off();
        break;
    case Detector::STATE_WARNING:
        write(BRIGHTNESS, BRIGHTNESS / 3, 0);
        break;
    default:
        write(0, BRIGHTNESS, 0);
        break;
    }
}

#endif
