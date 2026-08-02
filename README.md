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

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to the additional Board Manager URLs.
3. Install **esp32 by Espressif Systems 3.3.11**.
4. Install M5Unified 0.2.19, FastLED 3.10.5, ArduinoQueue 1.2.5, and ArduinoLog 1.1.1.
5. Select **M5Atom** as the board (`esp32:esp32:m5stack_atom` in Arduino CLI).
6. Open `src/bb-link/bb-link.ino`, compile, and flash the ATOM Lite.
7. Install [B.B. Link Configurator](https://apps.apple.com/us/app/b-b-link-configurator/id6476163710) and [RadioMail](https://radiomail.app) on the iPhone or iPad.

The firmware advertises the compatibility name `B.B. Link` by default. A custom name can be supplied at build time with `ADAPTER_NAME`, but application-side rig-control detection must be verified if the name is changed.

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
  - Short press: Disconnect and retry the saved radio connection.
  - Hold for 2 seconds while running: enter deep sleep. Press once to wake.
  - Hold for 3 seconds during startup: request the five-minute BLE firmware-update window. The service is exposed only when the build permits signed OTA, or when unsigned physical-access OTA was explicitly enabled at build time.

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

## Firmware Update Security

The BLE OTA service is not advertised during normal operation. It requires a three-second physical button hold during startup, uses an encrypted BLE characteristic, closes after five minutes, and checks every ESP-IDF OTA operation for failure. New firmware is marked valid only after the bridge, Bluetooth Classic, BLE, preferences, button, and status indicator have initialized successfully.

Unsigned OTA is disabled in source builds by default. Production OTA builds should use ESP-IDF signed-app verification (`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`) and bootloader rollback (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`). Developers who accept the reduced security of a physically authorized unsigned update may explicitly build with `BB_LINK_ALLOW_UNSIGNED_OTA_WITH_PHYSICAL_ACCESS=1`.

ATOM Lite uses hardware board ID `3`. Its update manifests are published under `atomlite/` on the `ota-firmware` branch. The corresponding Configurator mapping is:

```swift
case 0x03:
  return URL(string: "https://raw.githubusercontent.com/halka/bb-link/ota-firmware/atomlite/\(channel).json")
```

The release workflow builds only the ATOM Lite target and explicitly enables the physical-access unsigned OTA profile. It creates `bb-link-atomlite-<version>.bin`, its SHA-256 checksum, and the beta manifest. The separate **Promote OTA beta** workflow promotes `beta.json` to `latest.json` after validation. For higher-assurance deployments, replace that profile with a custom ESP-IDF core configured for signed-app verification and rollback.

## Tests

The KISS stream parser tests run without Arduino or EpoxyDuino:

```sh
make -C tests runtests
```

They cover offset frames, malformed and truncated commands, escaped data, multiple frames in one BLE write, and commands split across writes. The legacy hardware-abstraction tests remain available as `epoxy-tests` and accept `EPOXYDUINO_DIR` instead of relying on a fixed home-directory path.

## Troubleshooting

If the adapter connects to the radio but the radio does not transmit, check the TNC settings. Go to Menu > Configuration > Interface > KISS (983) and set it to Bluetooth.

## How to Contribute

This project is open source, so everyone's contribution is welcome. Here's a quick guide to get started:

- **Share**: If you build your own adapter, share it online! Post photos, write a blog post, or create a tutorial video to show others how it's done.
- **Update Documentation**: Help improve or correct the documentation. Fork the repo, make your updates, and submit a pull request.
- **Submit Change Requests**: If you're developing a new feature or bug fix, fork the repository, create a new branch for your changes, and submit a pull request with a clear description of your modifications.
- **Write Good Issue Reports**: If you encounter bugs or have feature suggestions, please submit an issue report with a clear title, a detailed description, and steps to reproduce the issue if it's a bug.

The source code for the [Configurator](https://github.com/islandmagic/ios-bblink-config) app is available as well.
