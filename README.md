# ☕ PourBot

> **An open-source smart pour-over coffee scale built for precision brewing.**

PourBot is a DIY smart coffee scale designed for coffee enthusiasts who want the features of a premium commercial scale while enjoying the flexibility of an open-source project.

Built around the **Waveshare ESP32-S3 Touch AMOLED**, PourBot combines precision weighing, an intuitive touchscreen interface, rechargeable battery operation, and browser-based firmware installation into one compact package.

---

## ✨ Features

### ☕ Assisted Brewing

PourBot guides you through every pour, helping you brew consistently exceptional coffee with confidence.

### 🎯 Brew Progress Ring

The animated center progress ring provides instant visual feedback, showing exactly how close you are to your target weight.

### ⚖️ Precision Calibration

Built-in calibration allows you to fine-tune the scale for consistent **0.1 g accuracy**.

### 🔋 Battery Powered

Designed specifically for the Waveshare ESP32-S3 platform with integrated battery charging and real-time battery monitoring for truly portable brewing.

### 🌐 Browser-Based Configuration

Access PourBot's web interface for advanced settings, calibration, firmware management, and additional features.

### 🚀 One-Click Firmware Updates

Update your scale directly from Chrome or Microsoft Edge using ESP Web Tools—no Arduino IDE required.

---

# Hardware

PourBot is built around a small collection of readily available components.

| Component                             | Purpose                                 |
| ------------------------------------- | --------------------------------------- |
| Waveshare ESP32-S3 Touch AMOLED 1.64" | Main controller and touchscreen display |
| HX711 Load Cell Amplifier             | High-resolution weight measurement      |
| 1 kg Bar Style Load Cell              | Precision weighing sensor               |
| Single Cell LiPo Battery              | Portable power                          |
| 3D Printed Enclosure                  | Houses all electronics                  |

---

# Build Guide

Building PourBot is straightforward.

1. Print the enclosure.
2. Install the load cell.
3. Mount the brew platform.
4. Wire the load cell to the HX711.
5. Connect the HX711 to the ESP32-S3.
6. Install the AMOLED display.
7. Connect the LiPo battery.
8. Flash the firmware.
9. Calibrate the scale.
10. Brew coffee!

---

# Firmware Installation

Firmware can be installed directly from your browser using **ESP Web Tools**.

Simply:

1. Connect the ESP32-S3 with a USB-C data cable.
2. Open the project website.
3. Click **Install Firmware**.
4. Select the correct serial port.
5. Wait for flashing to complete.

No Arduino IDE or PlatformIO installation is required.

---

# Current Features

* Precision weight measurement
* 0.1 g resolution
* Brew timer
* Assisted brewing mode
* Animated brew progress ring
* Tare controls
* Touchscreen interface
* Rechargeable battery support
* Battery monitoring
* Wi-Fi connectivity
* Browser-based settings
* Browser firmware updates
* Built-in scale calibration

---

# Roadmap

Future ideas include:

* Recipe library
* Brew profile storage
* Automatic brew logging
* Bluetooth integration
* Cloud synchronization
* Additional brewing modes
* Advanced brew analytics

---

# Open Source

PourBot is an open-source project created for the coffee and maker communities.

Contributions, bug reports, feature requests, and pull requests are always welcome.

---

# License

This project is released under the MIT License.

---

## Happy Brewing! ☕

If you build a PourBot, we'd love to see it. Share your builds, modifications, and brewing setups with the community!

