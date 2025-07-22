
/*Project: MagSenseUI
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
      - Configuration storage
      - Serial output mode for live sensor data
      - Auto timeout to proceed with saved or default configuration
    
      Modified for ESP32-S3 Super Mini
  
    Pin Configuration:
      - Buttons: UP(13), DOWN(14), SET(15)
      - I2C: SDA(11), SCL(12)
      
    Configuration: Arduino Serial Port: Serial 115200
*/


#include <Wire.h>
#include <U8g2lib.h>
#include "Adafruit_MLX90393.h"
#include <EEPROM.h>

// OLED display configuration (using specified I2C pins)
U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 12, /* data=*/ 11);

// Magnetic sensor instance
Adafruit_MLX90393 magneticSensor;

// Button pins for ESP32-S3 Super Mini
const int BUTTON_UP = 10;
const int BUTTON_DOWN = 11;
const int BUTTON_SELECT = 12;

// Menu options
const char* const gainLabels[] = {
  "1X", "1.33X", "1.67X", "2X", "2.5X",
  "3X", "4X", "5X", "Serial ON"
};
const mlx90393_gain gainValues[] = {
  MLX90393_GAIN_1X, MLX90393_GAIN_1_33X, MLX90393_GAIN_1_67X,
  MLX90393_GAIN_2X, MLX90393_GAIN_2_5X, MLX90393_GAIN_3X,
  MLX90393_GAIN_4X, MLX90393_GAIN_5X
};
const int TOTAL_OPTIONS = 9;
const int visibleMenuOptions = 5;

// Menu state variables
int currentOption = 0;
int menuScrollOffset = 0;
bool isConfigured = false;
bool serialModeActive = false;
bool inMenuMode = true;

// Timing control
unsigned long menuStartTime = 0;
const unsigned long MENU_TIMEOUT = 5000; // 5s timeout
unsigned long lastMeasurementTime = 0;
const unsigned long MEASUREMENT_INTERVAL = 500;

// Sensor reading variables
float magX, magY, magZ;
bool readSuccess = false;

// Debounce variables for buttons
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
  
  // Initialize I2C with custom pins
  Wire.begin(9, 10);
  
  display.begin();
  display.enableUTF8Print();

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  showIntro();
  display.setFont(u8g2_font_ncenB08_tr);

  // Initialize EEPROM for ESP32
  EEPROM.begin(1); // We only need 1 byte for our configuration
  
  // Load saved option from EEPROM
  int savedOption = EEPROM.read(0);
  if (savedOption >= 0 && savedOption < TOTAL_OPTIONS) {
    currentOption = savedOption;
  }
  adjustScroll();

  if (!magneticSensor.begin_I2C()) {
    showErrorMessage("Sensor not found");
    while(1);
  }

  // Configure sensor resolutions, oversampling and filter
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
    handleMenuNavigation(); // Handle user input in the configuration menu
  } else {
    handleSensorReadings(); // Read sensor and display or send data via serial
    checkMenuReturn(); // Check if SELECT button pressed to return to menu
  }
}

// Display introductory splash screen
void showIntro() {
  display.firstPage();
  do {
    display.setFont(u8g2_font_logisoso20_tr);
    display.drawStr(4, 40, "Fran-Byte");
  } while (display.nextPage());
  delay(1000);
}

// Display error message and halt program
void showErrorMessage(const char* msg) {
  display.firstPage();
  do {
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(5, 30, msg);
  } while(display.nextPage());
  while(1);
}

// Adjust the menu scroll to keep current option visible
void adjustScroll() {
  if (currentOption < menuScrollOffset) {
    menuScrollOffset = currentOption;
  } else if (currentOption >= menuScrollOffset + visibleMenuOptions) {
    menuScrollOffset = currentOption - visibleMenuOptions + 1;
  }
}

// Draw the configuration menu on the display
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

// Show a summary of the chosen configuration before starting measurement
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
      display.print("Gain: ");
      display.print(gainLabels[currentOption]);
    }
  } while(display.nextPage());
  delay(2000);
}

// Read and debounce the button states
void readButtons() {
  unsigned long currentTime = millis();

  // UP button debounce
  int readingUp = digitalRead(BUTTON_UP);
  if (readingUp != lastReadingUp) lastDebounceTimeUp = currentTime;
  if ((currentTime - lastDebounceTimeUp) > debounceDelay) stableButtonUpState = readingUp;
  lastReadingUp = readingUp;

  // DOWN button debounce
  int readingDown = digitalRead(BUTTON_DOWN);
  if (readingDown != lastReadingDown) lastDebounceTimeDown = currentTime;
  if ((currentTime - lastDebounceTimeDown) > debounceDelay) stableButtonDownState = readingDown;
  lastReadingDown = readingDown;

  // SELECT button debounce
  int readingSelect = digitalRead(BUTTON_SELECT);
  if (readingSelect != lastReadingSelect) lastDebounceTimeSelect = currentTime;
  if ((currentTime - lastDebounceTimeSelect) > debounceDelay) stableButtonSelectState = readingSelect;
  lastReadingSelect = readingSelect;
}

// Handle navigation and selection inside the configuration menu
void handleMenuNavigation() {
  static bool needsRedraw = true;
  bool buttonPressed = false;
  unsigned long currentTime = millis();

  readButtons();

  // Navigate up in menu
  if (stableButtonUpState == LOW && lastButtonUpState == HIGH) {
    currentOption = (currentOption == 0) ? TOTAL_OPTIONS - 1 : currentOption - 1;
    adjustScroll();
    buttonPressed = true;
    menuStartTime = currentTime;
  }

  // Navigate down in menu
  if (stableButtonDownState == LOW && lastButtonDownState == HIGH) {
    currentOption = (currentOption + 1) % TOTAL_OPTIONS;
    adjustScroll();
    buttonPressed = true;
    menuStartTime = currentTime;
  }

  // Select current option
  if (stableButtonSelectState == LOW && lastButtonSelectState == HIGH) {
    if (currentOption == 8) { // "Serial ON"
      serialModeActive = true;
    } else {
      serialModeActive = false;
      EEPROM.write(0, currentOption);
      EEPROM.commit(); // Required for ESP32 to save to EEPROM
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

  // Exit menu automatically after timeout
  if (currentTime - menuStartTime > MENU_TIMEOUT) {
    isConfigured = true;
    inMenuMode = false;
    showConfigurationSummary();
    return;
  }

  // Redraw menu if needed
  if (buttonPressed || needsRedraw) {
    showConfigurationMenu();
    needsRedraw = false;
  }
}

// Check if SELECT button is pressed to return to menu during measurement
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

// Return to configuration menu from measurement mode
void returnToMenu() {
  inMenuMode = true;
  menuStartTime = millis();
  showConfigurationMenu();
}

// Read sensor data and display or send via serial
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
          // Mark large changes with '>'
          char markX = abs(magX - prevX) > 25 ? '>' : ' ';
          char markY = abs(magY - prevY) > 25 ? '>' : ' ';
          char markZ = abs(magZ - prevZ) > 25 ? '>' : ' ';

          display.setCursor(12, 15);
          display.print("X: "); display.print(magX, 1); display.print(" uT "); display.print(markX);
          display.setCursor(12, 30);
          display.print("Y: "); display.print(magY, 1); display.print(" uT "); display.print(markY);
          display.setCursor(12, 45);
          display.print("Z: "); display.print(magZ, 1); display.print(" uT "); display.print(markZ);
          display.setCursor(12, 60);
          display.print("Gain: ");
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
