

# MagSenseUI

## 🎯 Project Purpose

The **MagSenseUI** project was developed to address a specific diagnostic challenge in electromechanical systems—particularly in machines like **Bill Validators** that rely on magnetic sensors for operation.

The program includes an intuitive **menu** system that allows users to configure the **sensor gain** and choose the data output mode. Users can select whether the magnetic field readings are displayed directly on the OLED screen or streamed via the Arduino's **serial port** to a connected PC. This flexibility makes it easy to adapt the system for both standalone field diagnostics and detailed data logging or analysis on a computer.

| **Select option:** |
|--------------------|
| 1X                 |
| 1.33X              |
| 1.67X              |
| 2X                 |
| 2.5X               |
| 3X                 |
| 4X                 |
| 5X                 |
| Serial ON          |



### 🧩 Problem Context

These machines often operate in environments where **strong neodymium magnets** are permanently present. While these magnets are essential to the machine’s function, they also create a **constant magnetic field** that can interfere with nearby components.

One such component is the **metallic bearing**. Under normal conditions, the bearing is non-magnetic. However, if it becomes **magnetized due to external contamination** (e.g., exposure to magnetic particles or prolonged proximity to strong fields), it can **distort the local magnetic environment**. This distortion may lead to **false readings or operational errors** in the machine’s magnetic sensors.

### 🧪 Why MagSenseUI?

While a traditional compass can detect strong magnetization, it **fails to detect subtle or low-level magnetization**. This is where **MagSenseUI** excels.

- It uses the **MLX90393** high-resolution 3-axis magnetometer to **detect even slight magnetic anomalies**.
- The system allows the user to **select different gain levels**, enhancing sensitivity to weak fields.
- Through a simple OLED interface and serial output mode, it provides **real-time feedback** on the magnetic field strength in microteslas (µT).
- This enables technicians to **identify which bearing is magnetized**, even if the magnetization is minimal and invisible to conventional tools.



## Hardware Components
- Arduino Nano
- OLED Display 1.3" (SH1106)
- MLX90393 Magnetic Sensor
- Push Buttons (x3)



---

## 🔧 Technical Specifications

### 🧠 Arduino Nano
The **Arduino Nano** is a compact, breadboard-friendly microcontroller board based on the **ATmega328P**. It is ideal for small-scale embedded projects.

- **Microcontroller**: ATmega328P
- **Operating Voltage**: 5V
- **Input Voltage (recommended)**: 7–12V
- **Digital I/O Pins**: 14 (6 PWM outputs)
- **Analog Input Pins**: 8
- **Flash Memory**: 32 KB (2 KB used by bootloader)
- **SRAM**: 2 KB
- **EEPROM**: 1 KB
- **Clock Speed**: 16 MHz
- **Communication**: UART, I2C (A4/A5), SPI
- **USB Interface**: Mini USB (via CH340 or FT232)

<img src="nano.jpg" width="40%">

---

### 🧲 MLX90393 Magnetic Sensor
The **MLX90393** is a highly sensitive 3-axis magnetometer designed for precision magnetic field sensing.

- **Measurement Axes**: X, Y, Z
- **Output**: Magnetic field in microteslas (µT)
- **Resolution**: Configurable (up to 0.161 µT/LSB)
- **Gain Settings**: 1X to 5X (selectable)
- **Interface**: I2C (also supports SPI)
- **I2C Address**: 0x0C (default)
- **Supply Voltage**: 2.2V – 3.6V
- **Current Consumption**: ~2.5 mA (active), ~1 µA (standby)
- **Features**:
  - Programmable gain, resolution, oversampling, and filtering
  - High sensitivity for detecting small magnetic variations
  - Ideal for contactless position sensing and field mapping

<img src="mlx90393.jpg" width="50%">

---

### 🖥️ OLED Display 1.3" (SH1106)
This **1.3-inch OLED** display uses the **SH1106** driver and communicates via I2C. It is perfect for compact UIs in embedded systems.

- **Resolution**: 128 × 64 pixels
- **Display Type**: Monochrome OLED
- **Driver IC**: SH1106
- **Interface**: I2C (SDA/SCL)
- **Operating Voltage**: 3.3V – 5V
- **Power Consumption**: Low (no backlight needed)
- **Viewing Angle**: >160°
- **Library Support**: U8g2, Adafruit SH1106, etc.

<img src="oled.jpg" width="30%">



## Wiring Diagram
Connect components via I2C:
- **OLED**: SDA → A4, SCL → A5  
- **MLX90393**: SDA → A4, SCL → A5  
- **Buttons**:  
  - UP → D2  
  - DOWN → D3  
  - SELECT → D4  

![circuit](mg_board.jpg)

## Usage
1. Upload the `MagSenseUI.ino` sketch to your Arduino Nano.
2. Use the buttons to navigate the OLED menu and select gain settings.
3. Configuration is saved to EEPROM.
4. Optionally activate Serial Mode to stream magnetic field data (X, Y, Z in µT).

CODE: [this](code.ino)
