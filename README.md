# B.B. Link, the BLE to Bluetooth Classic adapter for Kenwood TH-D74/5 Radios on M5Stack ATOM-Lite
<img src="https://shop.m5stack.com/cdn/shop/products/3_2e4a5d8d-c739-405a-9494-431e2edec8ae_1200x1200.jpg?v=1655692121" width="30%">

Some devices, like the Kenwood TH-D75 radio only support Bluetooth Classic serial profile. iOS devices only support Bluetooth Low Energy (BLE). They are not compatible and as such, you can't pair those devices together. This code provides a way to create an adapter that can interface a device that exposes a serial profile over Bluetooth Classic, to an iOS device via BLE. Its main purpose is to enable iOS app that supports AX.25 packet like RadioMail or APRS.fi to use the TNC built in the radio as a modem.

For a detailed "how-to build" this adapter, watch this video: 

[![Watch the video](https://image.mux.com/SZQsZnBJDJf4GMUrTRxJ386tSpsIlRP02yfmXZr79TKg/thumbnail.png?time=1200)](https://player.mux.com/SZQsZnBJDJf4GMUrTRxJ386tSpsIlRP02yfmXZr79TKg)

After you're done, come back and follow the instructions here as they are continually updated.

## Hardware

The adapter is based on the ESP32 microcontroller, which provides support for both Bluetooth Classic and Bluetooth Low Energy (BLE).

### Material

1. M5Stack ATOM-Lite [Buy](https://shop.m5stack.com/products/m5atom-lite-v2-0)
2. External USB power pack
3. USB-C cable

### Power

The adapter can be powered by a USB power source, such as a USB adapter or a portable power bank. It can also be powered by an external USB power pack for use on the go.

## Software

1. Install Arduino IDE 2.x [https://www.arduino.cc](https://www.arduino.cc)
1. Add additional library source in Arduino IDE. Settings > Additional board manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
1. Install the esp32 by Espressif Systems board library
1. Install the M5Unified library
1. Install FastLED library
1. Install FreeRTOS library
1. Install ArduinoQueue library
1. Install ArduinoLog library
1. Clone this repo
1. Flash the code to the M5Stack ATOM-Lite board
1. Download [B.B. Link Configurator](https://apps.apple.com/us/app/b-b-link-configurator/id6476163710) app on your phone
1. Download RadioMail on your phone [https://radiomail.app](https://radiomail.app)

### Rig Control

By default, the adapter will set the radio to KISS mode and automatically respond to RadioMail's instructions to switch frequencies, enabling seamless operation. If you prefer the adapter not to alter radio settings during use, toggle off the 'Control Frequency' option in the configurator app.

## Operating Instructions

### Powering
#### On
Connect the adapter to a USB power source.

#### Off
Disconnect the adapter from the USB power source.

### Pairing with a Radio (one time only)

1. Turn on adapter
1. Open B.B. Link Configurator app on your  phone
1. After a few seconds, `B.B. Link` appears in the list of nearby adapters. Select it.
1. Set the radio in Bluetooth discovery mode. Navigate to 'Menu -> Bluetooth -> Pairing Mode' on the radio.
1. Tap 'Paired Radio' on B.B. Link app
1. Wait a few seconds, the name of your radio should appear in the list. Select it.
1. After a few seconds a PIN prompt should appear on the radio. Press 'OK' to accept. This step is only necessary once; afterwards, the adapter will automatically try to reconnect with the radio.
1. Successful pairing is indicated by a breathing blue LED on the adapter.

### Pairing with iPhone or iPad

1. Make sure B.B. Link Configurator app is fully closed
1. Open the RadioMail app. Proceed to 'Settings -> KISS TNC Modem -> Default TNC'.
1. `B.B. Link` should be visible in the discovery screen.
1. Select `B.B. Link` and tap 'Done'.
1. Navigate to the connection screen and choose a packet station.
1. A solid blue LED on the adapter signals that RadioMail has connected. Red and green LEDs will flash to indicate data transmission and reception.

### User Interface
#### LED Indicator

The dotstar tri-color LED is used to indicate various states:

- 🟠: Idle, adapter waiting to pair
- 🔵 (slow flash): Adapter scanning for radio
- 🔵 (breathing): Idle, paired with radio
- 🔵: Ready, radio and iOS device paired
- 🟠 (fast blink): Shutting down

Data Activity:

- 🟢: Rx (Receiving)
- 🔴: Tx (Transmitting)
- 🟣: Rx/Tx (Both Receiving and Transmitting)

Error Conditions:

- 🔴 (fast blink): Low battery immediate shutdown
- 🔴 (slow flash): Fatal error, must reset

### Factory Reset

You can reset the adapter to its default configuration. This will clear the list of previously paired devices and restoring default settings. Simply tap 'Reset Adapter' in the configurator app.

Alternatively, you can reset the adapter by connecting it to a computer. Here's how to do it:

1. Launch the Arduino IDE on your PC.
1. Connect the adapter to your PC using a USB cable.
1. Navigate to the Serial Monitor within the IDE.
1. Type `R` into the Serial Monitor and send the command.
1. Monitor the response in the Serial Monitor which will confirm the clearing of the previously paired devices.
1. After the process is complete, disconnect the adapter from your PC.

## Troubleshooting

* If the adapter connects to the radio but the radio does not transmit, check the TNC settings. Go to Menu > Configuration > Interface > KISS (983) and set it to Bluetooth.

## How to Contribute

This project is open source, so everyone's contribution is welcome. Here's a quick guide to get started:

* **Share**: If you build your own adapter, share it online! Post photos, write a blog post, or create a tutorial video to show others how it's done.
* **Update Documentation**: Help improve or correct the documentation. Fork the repo, make your updates, and submit a pull request.
* **Submit Change Requests**: If you're developing a new feature or bug fix, fork the repository, create a new branch for your changes, and submit a pull request with a clear description of your modifications.
* **Write Good Issue Reports**: If you encounter bugs or have feature suggestions, please submit an issue report with a clear title, a detailed description, and steps to reproduce the issue if it's a bug.

The source code for the [Configurator](https://github.com/islandmagic/ios-bblink-config) app is available as well.

