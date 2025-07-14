/*
  Project: MagSenseUI
  Author: Fran-Byte
  Description:
    MagSenseUI is an interface designed for configuring and monitoring the MLX90393 magnetic sensor.
    It allows the user to select sensor gain levels via an OLED display and physical buttons.
    The system can also enter a Serial Mode to stream live magnetic field data (X, Y, Z in microteslas).

    Purpose:
      Under the influence of a strong magnetic field, the system aims to detect and discriminate
      small variations or secondary magnetic fields, allowing precise magnetic sensing applications.

    Features:
      - OLED menu for gain selection
      - EEPROM-based configuration storage
      - Serial output mode for live sensor data
      - Auto timeout to proceed with saved or default configuration
      
    Configuration: Arduino Serial Port: Serial 115200
*/
#include <Wire.h>
#include <U8g2lib.h>
#include "Adafruit_MLX90393.h"
#include <EEPROM.h>

U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0);
Adafruit_MLX90393 magneticSensor;

#define BUTTON_UP     2
#define BUTTON_DOWN   3
#define BUTTON_SELECT 4

const char gainLabels0[] PROGMEM = "1X";
const char gainLabels1[] PROGMEM = "1.33X";
const char gainLabels2[] PROGMEM = "1.67X";
const char gainLabels3[] PROGMEM = "2X";
const char gainLabels4[] PROGMEM = "2.5X";
const char gainLabels5[] PROGMEM = "3X";
const char gainLabels6[] PROGMEM = "4X";
const char gainLabels7[] PROGMEM = "5X";
const char gainLabels8[] PROGMEM = "Serial ON";

const char *const gainLabels[] PROGMEM = {
  gainLabels0, gainLabels1, gainLabels2, gainLabels3, gainLabels4,
  gainLabels5, gainLabels6, gainLabels7, gainLabels8
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

const uint16_t TIMEOUT_MS = 3000;
uint16_t lastInteractionTime = 0;
bool interactionOccurred = false;

uint16_t lastMeasurementTime = 0;
const uint16_t MEASUREMENT_INTERVAL = 500;

float magX, magY, magZ;
bool readSuccess = false;

const int visibleMenuOptions = 5;
int menuScrollOffset = 0;

uint16_t lastButtonUpTime = 0;
uint16_t lastButtonDownTime = 0;
uint16_t lastButtonSelectTime = 0;
const uint16_t debounceDelay = 200;

// Print gain label stored in PROGMEM to save RAM
void printGainLabel(int index, int x, int y) {
  char buffer[12];
  strcpy_P(buffer, (char*)pgm_read_word(&(gainLabels[index])));
  display.setCursor(x, y);
  display.print(buffer);
}

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

// Main loop: handle menu or sensor reading based on config state
void loop() {
  if (!isConfigured) {
    handleMenu();
  } else {
    handleSensorReadings();
  }
}

// Adjust menu scroll offset to keep current selection visible
void adjustScroll() {
  if (currentOption < menuScrollOffset) {
    menuScrollOffset = currentOption;
  } else if (currentOption >= menuScrollOffset + visibleMenuOptions) {
    menuScrollOffset = currentOption - visibleMenuOptions + 1;
  }
}

// Handle button inputs and menu navigation
void handleMenu() {
  bool upPressed = (digitalRead(BUTTON_UP) == LOW);
  bool downPressed = (digitalRead(BUTTON_DOWN) == LOW);
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);
  uint16_t now = millis();
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

// Display the gain selection menu
void showConfigurationMenu() {
  display.firstPage();
  do {
    display.drawFrame(0, 0, 128, 64);
    display.drawStr(8, 10, "Select option:");
    for (int i = 0; i < visibleMenuOptions; i++) {
      int index = menuScrollOffset + i;
      if (index >= TOTAL_OPTIONS) break;
      int y = 20 + i * 10;
      if (index == currentOption) display.drawStr(0, y, ">");
      printGainLabel(index, 10, y);
    }
  } while (display.nextPage());
}

// Show summary after configuration is done
void showConfigurationSummary() {
  display.firstPage();
  do {
    display.setCursor(8, 15);
    display.print("Configuration:");
    display.setCursor(8, 30);

    if (currentOption == 8) {
      display.print("Serial: ON");
      display.setCursor(8, 45);
      display.print("Display: OFF");
    } else {
      display.drawFrame(0, 0, 128, 64);
      display.print("Gain: ");
      printGainLabel(currentOption, 60, 30);
    }
  } while (display.nextPage());

  delay(2000);
}

// Read sensor data and show on display or serial port
// Also detect sudden variations >= 10 µT and show "-X-", "-Y-", "-Z-" alerts
void handleSensorReadings() {
  static float prevMagX = 0, prevMagY = 0, prevMagZ = 0;
  uint16_t now = millis();

  if (now - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
    lastMeasurementTime = now;
    readSuccess = magneticSensor.readData(&magX, &magY, &magZ);

    if (serialModeActive && readSuccess) {
      Serial.print("X:  "); Serial.print(magX, 2); Serial.print(" uT\t");
      Serial.print("Y:  "); Serial.print(magY, 2); Serial.print(" uT\t");
      Serial.print("Z:  "); Serial.print(magZ, 2); Serial.println(" uT");
    }

    if (!serialModeActive || currentOption != 8) {
      display.firstPage();
      do {
        display.drawFrame(0, 0, 128, 64);

        if (readSuccess) {
          display.setCursor(12, 15);
          display.print("X : "); 
          display.print(magX, 1); 
          display.print(" uT");
          if (abs(magX - prevMagX) >= 10) display.print(" -X-");

          display.setCursor(12, 30);
          display.print("Y : "); 
          display.print(magY, 1); 
          display.print(" uT");
          if (abs(magY - prevMagY) >= 10) display.print(" -Y-");

          display.setCursor(12, 45);
          display.print("Z : "); 
          display.print(magZ, 1);
          display.print(" uT");
          if (abs(magZ - prevMagZ) >= 10) display.print(" -Z-");

          display.setCursor(12, 60);
          display.print("Gain: ");
          printGainLabel(currentOption, 60, 60);
        } else {
          display.setCursor(10, 30);
          display.print("Read error");
        }
      } while (display.nextPage());
    }

    // Save current readings for next comparison
    prevMagX = magX;
    prevMagY = magY;
    prevMagZ = magZ;
  }
}

// Show error message on display
void showErrorMessage(const char* message) {
  display.firstPage();
  do {
    display.drawStr(5, 30, message);
  } while (display.nextPage());
}
