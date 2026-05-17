# B.B. Link, the BLE to Bluetooth Classic Adapter for Kenwood TH-D74/5 Radios on M5Stack ATOM-Lite

## Objective
This code provides a way to create an adapter that interfaces a device exposing a serial profile over Bluetooth Classic with an iOS device via BLE, such as the Kenwood TH-D74/5 radios that only support Bluetooth Classic serial profiles.

To enable iOS apps that support AX.25 packets, like RadioMail or APRS.fi, to use the built-in TNC in the radio as a modem, note that iOS devices only support Bluetooth Low Energy (BLE). These protocols are incompatible, so you can't pair those devices directly.

**For a detailed "how-to build" this adapter, watch this video:**

[![Watch the video](https://image.mux.com/SZQsZnBJDJf4GMUrTRxJ386tSpsIlRP02yfmXZr79TKg/thumbnail.png?time=1200)](https://player.mux.com/SZQsZnBJDJf4GMUrTRxJ386tSpsIlRP02yfmXZr79TKg)

## For ATOM-Lite
This repository is optimized for ATOM-Lite.

[![ATOM LITE](https://raw.githubusercontent.com/halka/bb-link/refs/heads/master/assets/atomlite.webp)](https://docs.m5stack.com/en/core/ATOM%20Lite)

## Hardware

The adapter is based on the ESP32 microcontroller, which provides support for both Bluetooth Classic and Bluetooth Low Energy (BLE).

### Materials

1. M5Stack ATOM-Lite [Buy](https://shop.m5stack.com/products/m5atom-lite-v2-0)
2. USB power source: power bank, USB-C iPhone, USB-C iPad, or powered Lightning adapter
3. USB-C cable

### Power

The adapter can be powered by a USB power source, such as a USB adapter, portable power bank, USB-C iPhone, or USB-C iPad. USB-C iPhone/iPad direct power is supported by the firmware's default mobile power profile.

To power the adapter from an iPhone or iPad:

- USB-C iPhone/iPad: connect the adapter directly with a USB-C to USB-C cable.
- Lightning iPhone/iPad: use Apple's Lightning to USB Camera Adapter or Lightning to USB 3 Camera Adapter. If the device reports that the accessory needs too much power, connect external power to the adapter.

The firmware keeps the CPU clock, Bluetooth transmit power, and status LED brightness low by default so the adapter draws less current from mobile devices. This assumes the board is powered through the ATOM-Lite USB-C port; USB-C source detection itself is handled by the board hardware, not firmware.

## Build
1. Install Arduino IDE [https://www.arduino.cc](https://www.arduino.cc)
2. Add additional board manager URLs in Arduino IDE Settings: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Install the ESP32 board library by Espressif Systems
4. Install the M5Unified library
5. Install the FastLED library
6. Install the FreeRTOS library
7. Install the ArduinoQueue library
8. Install the ArduinoLog library
9. Clone this repository
10. Flash the code to the M5Stack ATOM-Lite board
11. Download the [B.B. Link Configurator](https://apps.apple.com/us/app/b-b-link-configurator/id6476163710) app on your phone
12. Download RadioMail on your phone [https://radiomail.app](https://radiomail.app)

### Rig Control

By default, the adapter sets the radio to KISS mode and automatically responds to RadioMail's instructions to switch frequencies, enabling seamless operation. If you prefer the adapter not to alter radio settings during use, toggle off the 'Control Frequency' option in the configurator app.

## Operating Instructions

### Powering On/Off
- **On**: Connect the adapter to a USB power source, including a USB-C iPhone or iPad.
- **Off**: Disconnect the adapter from the USB power source.

### Pairing with a Radio (One-Time Setup)

1. Turn on the adapter.
2. Open the B.B. Link Configurator app on your phone.
3. After a few seconds, `B.B. Link` appears in the list of nearby adapters. Select it.
4. Set the radio in Bluetooth discovery mode. Navigate to 'Menu -> Bluetooth -> Pairing Mode' on the radio.
5. Tap 'Paired Radio' in the B.B. Link app.
6. Wait a few seconds; the name of your radio should appear in the list. Select it.
7. After a few seconds, a PIN prompt should appear on the radio. Press 'OK' to accept. This step is only necessary once; afterwards, the adapter will automatically try to reconnect with the radio.
8. Successful pairing is indicated by a breathing blue LED on the adapter.

### Pairing with iPhone or iPad

1. Make sure the B.B. Link Configurator app is fully closed.
2. Open the RadioMail app. Proceed to 'Settings -> KISS TNC Modem -> Default TNC'.
3. `B.B. Link` should be visible in the discovery screen.
4. Select `B.B. Link` and tap 'Done'.
5. Navigate to the connection screen and choose a packet station.
6. A solid blue LED on the adapter signals that RadioMail has connected. Red and green LEDs will flash to indicate data transmission and reception.

## User Interface

### Buttons
- **Main Button**:
  - Long press: Power on/off
  - Short press: Reconnect to radio and/or iOS device
- **Side Button**:
  - Long press: Reboot the adapter

### LED Indicators

#### Adapter Status
| Color | Status |
|-------|--------|
| 🟡 | Idle, adapter waiting to pair |
| 🔵 (slow flash) | Adapter scanning for radio |
| 🔵 (breathing) | Idle, paired with radio |
| 🔵 | Ready, radio and iOS device paired |
| 🟡 (fast blink) | Shutting down |
| 🔴 (slow flash) | **Fatal error**, must reset |

#### Data Activity
| Color | Status |
|-------|--------|
| 🟢 | Rx (Receiving) |
| 🔴 | Tx (Transmitting) |
| 🟣 | Rx/Tx (Both Receiving and Transmitting) |

## Factory Reset

You can reset the adapter to its default configuration. This will clear the list of previously paired devices and restore default settings. Simply tap 'Reset Adapter' in the configurator app.

Alternatively, you can reset the adapter by connecting it to a computer:

1. Launch the Arduino IDE on your PC.
2. Connect the adapter to your PC using a USB cable.
3. Navigate to the Serial Monitor within the IDE.
4. Type `R` into the Serial Monitor and send the command.
5. Monitor the response in the Serial Monitor, which will confirm the clearing of previously paired devices.
6. After the process is complete, disconnect the adapter from your PC.

## Troubleshooting

If the adapter connects to the radio but the radio does not transmit, check the TNC settings. Go to Menu > Configuration > Interface > KISS (983) and set it to Bluetooth.

## How to Contribute

This project is open source, so everyone's contribution is welcome. Here's a quick guide to get started:

- **Share**: If you build your own adapter, share it online! Post photos, write a blog post, or create a tutorial video to show others how it's done.
- **Update Documentation**: Help improve or correct the documentation. Fork the repo, make your updates, and submit a pull request.
- **Submit Change Requests**: If you're developing a new feature or bug fix, fork the repository, create a new branch for your changes, and submit a pull request with a clear description of your modifications.
- **Write Good Issue Reports**: If you encounter bugs or have feature suggestions, please submit an issue report with a clear title, a detailed description, and steps to reproduce the issue if it's a bug.

The source code for the [Configurator](https://github.com/islandmagic/ios-bblink-config) app is available as well.
