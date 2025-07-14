# MagSenseUI

MagSenseUI is a simple OLED-based interface for configuring the MLX90393 magnetic sensor using an Arduino Nano. It supports gain selection, EEPROM storage, and real-time magnetic field data streaming via Serial.

## Hardware Components
- Arduino Nano
- OLED Display 1.3" (SH1106)
- MLX90393 Magnetic Sensor
- Push Buttons (x3)

## Wiring Diagram
Connect components via I2C:
- **OLED**: SDA → A4, SCL → A5  
- **MLX90393**: SDA → A4, SCL → A5  
- **Buttons**:  
  - UP → D2  
  - DOWN → D3  
  - SELECT → D4  

## Usage
1. Upload the `MagSenseUI.ino` sketch to your Arduino Nano.
2. Use the buttons to navigate the OLED menu and select gain settings.
3. Configuration is saved to EEPROM.
4. Optionally activate Serial Mode to stream magnetic field data (X, Y, Z in µT).

```CPP
#include <Wire.h>
#include <U8g2lib.h>
#include "Adafruit_MLX90393.h"
#include <EEPROM.h>

U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0);
Adafruit_MLX90393 magneticSensor;

#define BUTTON_UP     2
#define BUTTON_DOWN   3
#define BUTTON_SELECT 4

const char* gainLabels[] = {
  "1X", "1.33X", "1.67X", "2X", "2.5X", "3X", "4X", "5X", "Serial ON"
};

const mlx90393_gain gainValues[] = {
  MLX90393_GAIN_1X, MLX90393_GAIN_1_33X, MLX90393_GAIN_1_67X,
  MLX90393_GAIN_2X, MLX90393_GAIN_2_5X, MLX90393_GAIN_3X,
  MLX90393_GAIN_4X, MLX90393_GAIN_5X
};

const int TOTAL_OPTIONS = 9;
int currentOption = 0;
bool isConfigured = false;
bool serialModeActive = false;

const unsigned long TIMEOUT_MS = 3000;
unsigned long lastInteractionTime = 0;
bool interactionOccurred = false;

unsigned long lastMeasurementTime = 0;
const unsigned long MEASUREMENT_INTERVAL = 500;

float magX, magY, magZ;
bool readSuccess = false;

const int visibleMenuOptions = 5;
int menuScrollOffset = 0;

unsigned long lastButtonUpTime = 0;
unsigned long lastButtonDownTime = 0;
unsigned long lastButtonSelectTime = 0;
const unsigned long debounceDelay = 200;

void setup() {
  Serial.begin(115200);

  display.begin();
  display.enableUTF8Print();
  display.setFont(u8g2_font_ncenB08_tr);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  int savedOption = EEPROM.read(0);
  currentOption = (savedOption >= 0 && savedOption < 8) ? savedOption : 0;

  if (!magneticSensor.begin_I2C()) {
    showErrorMessage("Sensor not found");
    while (1);
  }

  magneticSensor.setGain(gainValues[currentOption]);
  magneticSensor.setResolution(MLX90393_X, MLX90393_RES_17);
  magneticSensor.setResolution(MLX90393_Y, MLX90393_RES_17);
  magneticSensor.setResolution(MLX90393_Z, MLX90393_RES_17);
  magneticSensor.setOversampling(MLX90393_OSR_3);
  magneticSensor.setFilter(MLX90393_FILTER_5);

  showConfigurationMenu();
}

void loop() {
  if (!isConfigured) {
    handleMenu();
  } else {
    handleSensorReadings();
  }
}

void adjustScroll() {
  if (currentOption < menuScrollOffset) {
    menuScrollOffset = currentOption;
  } else if (currentOption >= menuScrollOffset + visibleMenuOptions) {
    menuScrollOffset = currentOption - visibleMenuOptions + 1;
  }
}

void handleMenu() {
  bool upPressed = (digitalRead(BUTTON_UP) == LOW);
  bool downPressed = (digitalRead(BUTTON_DOWN) == LOW);
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);
  unsigned long now = millis();
  bool refreshDisplay = false;

  if (upPressed && now - lastButtonUpTime > debounceDelay) {
    currentOption = (currentOption - 1 + TOTAL_OPTIONS) % TOTAL_OPTIONS;
    adjustScroll();
    lastButtonUpTime = now;
    refreshDisplay = true;
    interactionOccurred = true;
    lastInteractionTime = now;
  }

  if (downPressed && now - lastButtonDownTime > debounceDelay) {
    currentOption = (currentOption + 1) % TOTAL_OPTIONS;
    adjustScroll();
    lastButtonDownTime = now;
    refreshDisplay = true;
    interactionOccurred = true;
    lastInteractionTime = now;
  }

  if (selectPressed && now - lastButtonSelectTime > debounceDelay) {
    if (currentOption == 8) {
      serialModeActive = true;
      isConfigured = true;
      Serial.println("\n\n--- SERIAL MODE ACTIVATED ---");
      Serial.println("Format: X[uT]  Y[uT]  Z[uT]");
      Serial.println("----------------------------");
    } else {
      isConfigured = true;
      EEPROM.write(0, currentOption);
      magneticSensor.setGain(gainValues[currentOption]);
    }
    showConfigurationSummary();
    lastButtonSelectTime = now;
    return;
  }

  if (!interactionOccurred && now - lastInteractionTime >= TIMEOUT_MS) {
    if (currentOption >= 8) currentOption = 0;
    isConfigured = true;
    EEPROM.write(0, currentOption);
    showConfigurationSummary();
    return;
  }

  if (refreshDisplay) {
    showConfigurationMenu();
  }
}

void showConfigurationMenu() {
  display.firstPage();
  do {
    display.drawStr(0, 8, "Select option:");
    for (int i = 0; i < visibleMenuOptions; i++) {
      int index = menuScrollOffset + i;
      if (index >= TOTAL_OPTIONS) break;
      int y = 20 + i * 10;
      if (index == currentOption) display.drawStr(0, y, ">");
      display.setCursor(10, y);
      display.print(gainLabels[index]);
    }
  } while (display.nextPage());
}

void showConfigurationSummary() {
  display.firstPage();
  do {
    display.setCursor(0, 15);
    display.print("Configuration:");
    display.setCursor(0, 30);

    if (currentOption == 8) {
      display.print("Serial: ON");
      display.setCursor(0, 45);
      display.print("Display: OFF");
    } else {
      display.print("Gain: ");
      display.setCursor(60, 30);
      display.print(gainLabels[currentOption]);
    }
  } while (display.nextPage());

  delay(2000);
}

void handleSensorReadings() {
  unsigned long now = millis();

  if (now - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
    lastMeasurementTime = now;
    readSuccess = magneticSensor.readData(&magX, &magY, &magZ);

    if (serialModeActive && readSuccess) {
      Serial.print("X: "); Serial.print(magX, 2); Serial.print(" uT\t");
      Serial.print("Y: "); Serial.print(magY, 2); Serial.print(" uT\t");
      Serial.print("Z: "); Serial.print(magZ, 2); Serial.println(" uT");
    }

    if (!serialModeActive || currentOption != 8) {
      display.firstPage();
      do {
        if (readSuccess) {
          display.setCursor(0, 15);
          display.print("X: "); display.print(magX, 1); display.print(" uT");
          display.setCursor(0, 30);
          display.print("Y: "); display.print(magY, 1); display.print(" uT");
          display.setCursor(0, 45);
          display.print("Z: "); display.print(magZ, 1); display.print(" uT");
          display.setCursor(0, 60);
          display.print("Gain: ");
          display.print(gainLabels[currentOption]);
        } else {
          display.setCursor(0, 30);
          display.print("Read error");
        }
      } while (display.nextPage());
    }
  }
}

void showErrorMessage(const char* message) {
  display.firstPage();
  do {
    display.drawStr(0, 30, message);
  } while (display.nextPage());
}
```
