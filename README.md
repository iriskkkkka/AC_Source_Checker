# AC Source Checker

An ESP32-based AC power monitoring system with real-time HDMI display output via FPGA processing.

## Overview

This project implements a comprehensive AC power source monitoring system that measures current using the ACS712 sensor module, processes the data with an ESP32 microcontroller, and displays real-time information on an HDMI display through a Gowin Tang Nano 9K FPGA board. The system includes relay control for load switching and demonstrates hardware integration between microcontroller and FPGA platforms.

## Demo

[Demo](transfomer.mp4)

## System Architecture

### Hardware Components

- **ESP32 Microcontroller**: Main processing unit
  - Reads analog data from ACS712 current sensor
  - Controls relay module for load switching
  - Transmits processed data via UART to FPGA
  - Programmed using ESP-IDF framework

- **ACS712 Current Sensor Module**: AC current measurement
  - Provides analog voltage output proportional to AC current
  - Connected to ESP32 ADC input
  - Enables non-invasive current monitoring

- **Relay Module**: Load control
  - Allows switching of AC loads
  - Controlled by ESP32 GPIO
  - Provides isolation between control and power circuits

- **Gowin Tang Nano 9K FPGA**: Display controller
  - Receives sensor data via UART from ESP32
  - Processes and formats data for display
  - Drives HDMI output module
  - Handles real-time display updates

- **HDMI Display Module**: Visual output
  - Connected to Tang Nano 9K FPGA
  - Shows current measurements and system status
  - Provides real-time monitoring interface

## Features

- Real-time AC current monitoring
- Relay-controlled load switching
- HDMI display output for visual monitoring
- UART communication between ESP32 and FPGA
- High-speed FPGA-based display processing

## Hardware Requirements

- ESP32 development board
- ACS712 current sensor module (5A/20A/30A variant)
- Relay module (compatible with ESP32 logic levels)
- Gowin Tang Nano 9K FPGA board
- HDMI display module
- HDMI monitor/display
- Power supply (appropriate for your AC monitoring requirements)
- Connecting wires and breadboard/PCB

## Software Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) (Espressif IoT Development Framework)
- Gowin EDA software for FPGA development
- Python 3.x (for ESP-IDF tools)

## Usage

1. Power on the system
2. Connect the AC load to be monitored through the ACS712 sensor
3. The HDMI display will show real-time current measurements
4. Use the relay control to switch the load on/off (if implemented in your code)

## Configuration

### ESP32 Configuration
- ADC pin assignment
- UART baud rate (default: 115200)
- Relay control GPIO
- Sampling rate and averaging

### FPGA Configuration
- UART baud rate (must match ESP32)
- Display resolution and format
- Update refresh rate

## Acknowledgments

- Espressif Systems for ESP-IDF framework
- Gowin Semiconductor for Tang Nano 9K development tools
- ACS712 sensor module manufacturers

## References

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ACS712 Datasheet](https://www.allegromicro.com/en/products/sense/current-sensor-ics/zero-to-fifty-amp-integrated-conductor-sensor-ics/acs712)
- [Gowin Tang Nano 9K Documentation](https://wiki.sipeed.com/hardware/en/tang/Tang-Nano-9K/Nano-9K.html)

## Contact

For questions or issues, please open an issue in this repository.
