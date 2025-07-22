#include <Wire.h>
#include <U8g2lib.h>
#include "Adafruit_MLX90393.h"
#include <EEPROM.h>

U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 10, /* data=*/ 9);
Adafruit_MLX90393 magneticSensor;

// Button pins
const int BUTTON_UP = 5;
const int BUTTON_DOWN = 6;
const int BUTTON_SELECT = 7;

// Menu configuration
const char* const gainLabels[] = {
  "Gain: 1X", "Gain: 1.33X", "Gain: 1.67X", "Gain: 2X", "Gain: 2.5X",
  "Gain: 3X", "Gain: 4X", "Gain: 5X", "Serial ON"
};
const mlx90393_gain gainValues[] = {
  MLX90393_GAIN_1X, MLX90393_GAIN_1_33X, MLX90393_GAIN_1_67X,
  MLX90393_GAIN_2X, MLX90393_GAIN_2_5X, MLX90393_GAIN_3X,
  MLX90393_GAIN_4X, MLX90393_GAIN_5X
};
const int TOTAL_OPTIONS = 9;
const int visibleMenuOptions = 5;

// System state
int currentOption = 0;
int menuScrollOffset = 0;
bool isConfigured = false;
bool serialModeActive = false;
bool inMenuMode = true;

// Timing
unsigned long menuStartTime = 0;
const unsigned long MENU_TIMEOUT = 5000;
unsigned long lastMeasurementTime = 0;
const unsigned long MEASUREMENT_INTERVAL = 500;

// Sensor data
float magX, magY, magZ;
bool readSuccess = false;

// Button debounce
unsigned long lastDebounceTimeUp = 0;
unsigned long lastDebounceTimeDown = 0;
unsigned long lastDebounceTimeSelect = 0;
const unsigned long debounceDelay = 50;
int stableButtonUpState = HIGH;
int stableButtonDownState = HIGH;
int stableButtonSelectState = HIGH;
int lastReadingUp = HIGH;
int lastReadingDown = HIGH;
int lastReadingSelect = HIGH;
int lastButtonUpState = HIGH;
int lastButtonDownState = HIGH;
int lastButtonSelectState = HIGH;

void setup() {
  Serial.begin(115200);
  Wire.begin(9, 10);
  display.begin();
  display.enableUTF8Print();

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  showIntro();
  display.setFont(u8g2_font_ncenB08_tr);

  EEPROM.begin(1);
  int savedOption = EEPROM.read(0);
  if (savedOption >= 0 && savedOption < TOTAL_OPTIONS) {
    currentOption = savedOption;
  }
  adjustScroll();

  if (!magneticSensor.begin_I2C()) {
    showErrorMessage("Sensor not found");
    while(1);
  }

  magneticSensor.setResolution(MLX90393_X, MLX90393_RES_17);
  magneticSensor.setResolution(MLX90393_Y, MLX90393_RES_17);
  magneticSensor.setResolution(MLX90393_Z, MLX90393_RES_17);
  magneticSensor.setOversampling(MLX90393_OSR_3);
  magneticSensor.setFilter(MLX90393_FILTER_5);

  inMenuMode = true;
  menuStartTime = millis();
  showConfigurationMenu();
}

void loop() {
  if (inMenuMode) {
    handleMenuNavigation();
  } else {
    handleSensorReadings();
    checkMenuReturn();
  }
}

void showIntro() {
  display.firstPage();
  do {
    display.setFont(u8g2_font_logisoso20_tr);
    display.drawStr(4, 40, "Fran-Byte");
  } while (display.nextPage());
  delay(1000);
}

void showErrorMessage(const char* msg) {
  display.firstPage();
  do {
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(5, 30, msg);
  } while(display.nextPage());
  while(1);
}

void adjustScroll() {
  if (currentOption < menuScrollOffset) {
    menuScrollOffset = currentOption;
  } else if (currentOption >= menuScrollOffset + visibleMenuOptions) {
    menuScrollOffset = currentOption - visibleMenuOptions + 1;
  }
}

void showConfigurationMenu() {
  display.firstPage();
  do {
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawFrame(0, 0, 128, 64);
    display.drawStr(8, 10, "Select option:");
    for (int i = 0; i < visibleMenuOptions; i++) {
      int idx = menuScrollOffset + i;
      if (idx >= TOTAL_OPTIONS) break;
      int y = 20 + i * 10;
      if (idx == currentOption) display.drawStr(0, y, ">");
      display.setCursor(10, y);
      display.print(gainLabels[idx]);
    }
  } while(display.nextPage());
}

void showConfigurationSummary() {
  display.firstPage();
  do {
    display.setFont(u8g2_font_ncenB08_tr);
    display.setCursor(8, 15);
    display.print("Configuration:");
    display.setCursor(8, 30);
    if (serialModeActive) {
      display.print("Serial: ON");
      display.setCursor(8, 45);
      display.print("Display: OFF");
    } else {
      display.print(gainLabels[currentOption]);
    }
  } while(display.nextPage());
  delay(2000);
}

void readButtons() {
  unsigned long currentTime = millis();
  int readingUp = digitalRead(BUTTON_UP);
  if (readingUp != lastReadingUp) lastDebounceTimeUp = currentTime;
  if ((currentTime - lastDebounceTimeUp) > debounceDelay) stableButtonUpState = readingUp;
  lastReadingUp = readingUp;

  int readingDown = digitalRead(BUTTON_DOWN);
  if (readingDown != lastReadingDown) lastDebounceTimeDown = currentTime;
  if ((currentTime - lastDebounceTimeDown) > debounceDelay) stableButtonDownState = readingDown;
  lastReadingDown = readingDown;

  int readingSelect = digitalRead(BUTTON_SELECT);
  if (readingSelect != lastReadingSelect) lastDebounceTimeSelect = currentTime;
  if ((currentTime - lastDebounceTimeSelect) > debounceDelay) stableButtonSelectState = readingSelect;
  lastReadingSelect = readingSelect;
}

void handleMenuNavigation() {
  static bool needsRedraw = true;
  bool buttonPressed = false;
  unsigned long currentTime = millis();

  readButtons();

  if (stableButtonUpState == LOW && lastButtonUpState == HIGH) {
    currentOption = (currentOption == 0) ? TOTAL_OPTIONS - 1 : currentOption - 1;
    adjustScroll();
    buttonPressed = true;
    menuStartTime = currentTime;
  }

  if (stableButtonDownState == LOW && lastButtonDownState == HIGH) {
    currentOption = (currentOption + 1) % TOTAL_OPTIONS;
    adjustScroll();
    buttonPressed = true;
    menuStartTime = currentTime;
  }

  if (stableButtonSelectState == LOW && lastButtonSelectState == HIGH) {
    if (currentOption == 8) {
      serialModeActive = true;
    } else {
      serialModeActive = false;
      EEPROM.write(0, currentOption);
      EEPROM.commit();
      magneticSensor.setGain(gainValues[currentOption]);
    }

    isConfigured = true;
    inMenuMode = false;
    showConfigurationSummary();
    return;
  }

  lastButtonUpState = stableButtonUpState;
  lastButtonDownState = stableButtonDownState;
  lastButtonSelectState = stableButtonSelectState;

  if (currentTime - menuStartTime > MENU_TIMEOUT) {
    isConfigured = true;
    inMenuMode = false;
    showConfigurationSummary();
    return;
  }

  if (buttonPressed || needsRedraw) {
    showConfigurationMenu();
    needsRedraw = false;
  }
}

void checkMenuReturn() {
  int reading = digitalRead(BUTTON_SELECT);
  if (reading != lastButtonSelectState) {
    lastDebounceTimeSelect = millis();
  }
  if ((millis() - lastDebounceTimeSelect) > debounceDelay) {
    if (reading != stableButtonSelectState) {
      stableButtonSelectState = reading;
      if (stableButtonSelectState == LOW) {
        returnToMenu();
      }
    }
  }
  lastButtonSelectState = reading;
}

void returnToMenu() {
  inMenuMode = true;
  menuStartTime = millis();
  showConfigurationMenu();
}

void formatFixedWidthNumber(int x, int y, float value) {
  char buffer[10];
  
  // Format with exactly 5 characters (sign + 4 digits or space + 4 digits)
  if (value >= 0) {
    snprintf(buffer, sizeof(buffer), "%5.0f", value); // Space padding for positive numbers
  } else {
    snprintf(buffer, sizeof(buffer), "%5.0f", value); // Negative sign takes one position
  }
  
  display.setCursor(x, y);
  display.print(buffer);
}

int calculateBars(float delta) {
  if (delta >= 200) return 6;
  if (delta >= 100) return 4;
  if (delta >= 50) return 2;
  if (delta >= 25) return 1;
  return 0;
}

void drawVumeter(int x, int y, int bars) {
  for (int i = 0; i < bars; i++) {
    display.drawBox(x + (i * 4), y, 3, 8);
  }
}

void handleSensorReadings() {
  static float prevX = 0, prevY = 0, prevZ = 0;
  unsigned long now = millis();

  if (now - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
    lastMeasurementTime = now;
    readSuccess = magneticSensor.readData(&magX, &magY, &magZ);

    if (serialModeActive && readSuccess) {
      Serial.print("X: "); Serial.print(magX, 2); Serial.print(" uT\t");
      Serial.print("Y: "); Serial.print(magY, 2); Serial.print(" uT\t");
      Serial.print("Z: "); Serial.print(magZ, 2); Serial.println(" uT");
    }

    if (!serialModeActive) {
      display.firstPage();
      do {
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawFrame(0, 0, 128, 64);
        if (readSuccess) {
          float deltaX = abs(magX - prevX);
          float deltaY = abs(magY - prevY);
          float deltaZ = abs(magZ - prevZ);
          
          int barsX = calculateBars(deltaX);
          int barsY = calculateBars(deltaY);
          int barsZ = calculateBars(deltaZ);

          // X axis (fixed width display)
          display.setCursor(12, 15);
          display.print("X:");
          formatFixedWidthNumber(22, 15, magX);
          display.print(" uT  ");
          drawVumeter(76, 8, barsX);
          
          // Y axis (fixed width display)
          display.setCursor(12, 30);
          display.print("Y:");
          formatFixedWidthNumber(22, 30, magY);
          display.print(" uT  ");
          drawVumeter(76, 23, barsY);
          
          // Z axis (fixed width display)
          display.setCursor(12, 45);
          display.print("Z:");
          formatFixedWidthNumber(22, 45, magZ);
          display.print(" uT  ");
          drawVumeter(76, 38, barsZ);
          
          // Gain setting
          display.setCursor(12, 60);
          display.print(gainLabels[currentOption]);
        } else {
          display.setCursor(10, 30);
          display.print("Read error");
        }
      } while(display.nextPage());
    }

    prevX = magX;
    prevY = magY;
    prevZ = magZ;
  }
}
