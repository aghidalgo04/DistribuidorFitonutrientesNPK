# Smart Hydroponic Nutrient Monitoring System

The **Smart Hydroponic Nutrient Monitoring System** is an IoT-based embedded solution designed to detect and monitor essential phytonutrients—Nitrogen (N), Phosphorus (P), and Potassium (K)—in hydroponic solutions. By combining 18-channel Near-Infrared (NIR) spectroscopy with electroconductivity (EC) sensing, the system provides real-time analysis to optimize fertigation and reduce water waste.

## Overview

The primary objective of this project is to evaluate the viability of using the **AS7265x NIR sensor** to differentiate dissolved compounds in water. The system collects spectral data across 18 wavelengths (from UV to NIR) and correlates these signatures with nutrient concentrations. To ensure data accuracy, a **DFRobot EC sensor** provides a baseline of total dissolved solids, while an **ESP32** microcontroller orchestrates data collection and transmission.

The system integrates a complete IoT stack:
* **Edge Processing:** ESP32 handles I2C communication with the spectral triad and ADC sampling for the EC sensor.
* **Cloud Visualization:** Data is sent via MQTT to a **ThingsBoard** dashboard.
* **Alert System:** A rule-based engine triggers **Telegram** notifications and local LED signals when nutrient levels fall below safety thresholds.

## System Architecture

The following diagram illustrates the electronic interconnection and data flow between the sensors, the microcontroller, and the cloud platform.

### Component Diagram

![Component Diagram](https://github.com/aghidalgo04/DistribuidorFitonutrientesNPK/blob/main/imgs/ComponentDiagram.jpg)

**Diagram Description:** This schematic shows the wiring between the ESP32 and the AS7265x Spectral Triad via the I2C bus. It also details the connection of the DFRobot EC sensor to the analog-to-digital converter (ADC) pins and the status LEDs used for local alerts.

## Hardware Implementation

The hardware is housed in a controlled environment (dark box) to minimize interference from ambient light, which is critical for accurate NIR readings.

### Final Hardware Photo

![Hadware](https://github.com/aghidalgo04/DistribuidorFitonutrientesNPK/blob/main/imgs/HadwareNPK.jpg)

**Photo Description:** The prototype features an ESP32 mounted on a breadboard, connected to the AS7265x sensor and the EC probe submerged in a sample container. The setup includes status LEDs that toggle based on the detected concentration of nutrients.

## Key Features

### Advanced Spectral Sensing
* **18-Channel Analysis:** Utilizes the AS72651, AS72652, and AS72653 sensors to capture a wide spectrum (UV, Visible, and NIR).
* **Compound Differentiation:** Logic to distinguish between different dissolved substances (Salt, Sugar, and Bicarbonate used as test proxies for NPK).
* **Calibration Logic:** Interactive 2-point calibration for the EC sensor stored in NVS (Non-Volatile Storage).

### IoT & Remote Management
* **ThingsBoard Dashboard:** Real-time visualization of all 18 spectral channels and EC values using gauge and bar widgets.
* **Telegram Bot Integration:** Remote alerts sent directly to the user’s phone when critical values are detected.
* **OTA Updates:** Integrated Over-The-Air update capability to refresh firmware remotely via HTTPS.

### Power Management
* **Deep Sleep:** The system utilizes the ESP32's hibernation modes between readings to ensure energy efficiency, waking up periodically to transmit telemetry.

## Software Structure (C/ESP-IDF)

The project is modularized into specialized drivers and controllers:
* **main.c:** Manages WiFi/MQTT connectivity, SNTP time synchronization, and the main task orchestration.
* **as7265x.c:** Driver for the spectral triad, managing LED triggers and 18-channel data retrieval via I2C.
* **ec_sensor.c:** Handles ADC readings, voltage conversion, and NVS-based calibration for the EC probe.
* **i2c.c:** Low-level I2C master configuration and register read/write functions.
* **control_gpio.c:** Logic for triggering local hardware alarms (LEDs) based on sensor thresholds.

## Testing & Results

The system was tested using various solutions to verify spectral repeatability:
* **Repeatability:** Multiple measurements of the same sample showed consistent spectral "fingerprints."
* **Compound Detection:** Results indicated distinct peaks in the UV and NIR ranges for saline vs. sugar-based solutions, allowing the ThingsBoard rule chain to successfully categorize the samples.
* **Telemetry Stability:** Successful MQTT transmission even under low-power deep sleep cycles.

## Demonstration Video

You can watch the system in action, including the sensor response and ThingsBoard dashboard updates [here](https://www.youtube.com/watch?v=9InuSUNUnMc&t=3s).