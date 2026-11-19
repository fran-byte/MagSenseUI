#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <Arduino.h>

// Puerto serie de datos (salida de "Serial ON").
// En el ESP32-S3, "Serial" solo sale por el USB-C si en el IDE esta activado:
// Herramientas > USB CDC On Boot > Enabled
// Herramientas > USB Mode > Hardware CDC and JTAG (valor por defecto)
#if (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)) && \
    !(defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1))
#warning "USB CDC On Boot esta desactivado: Serial NO sale por el USB-C. Activar Herramientas > USB CDC On Boot > Enabled."
#endif

#define MAG_SERIAL Serial

#endif
