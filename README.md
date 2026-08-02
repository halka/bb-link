# B.B. Link for M5Stack ATOM Lite

B.B. Link bridges the Bluetooth Classic Serial Port Profile (SPP) of a
Kenwood TH-D74 or TH-D75 to Bluetooth Low Energy (BLE). This allows compatible
iPhone and iPad applications, including RadioMail, to use the radio's built-in
KISS TNC.

This fork targets the **M5Stack ATOM Lite**. The firmware advertises the
compatibility name `B.B. Link` and supports radio discovery, reconnect,
KISS-mode initialization, rig control, deep sleep, and factory reset.

## Table of contents

- [B.B. Link for M5Stack ATOM Lite](#bb-link-for-m5stack-atom-lite)
  - [Table of contents](#table-of-contents)
  - [Hardware](#hardware)
  - [Build and upload](#build-and-upload)
    - [Arduino IDE](#arduino-ide)
  - [Initial setup](#initial-setup)
    - [Pair the radio](#pair-the-radio)
    - [Connect RadioMail](#connect-radiomail)
  - [Usage](#usage)
    - [Controls](#controls)
    - [LED status](#led-status)
    - [Rig control](#rig-control)
    - [Factory reset](#factory-reset)
  - [Troubleshooting](#troubleshooting)
  - [Repository layout](#repository-layout)
  - [License and upstream project](#license-and-upstream-project)

## Hardware

- [M5Stack ATOM Lite](https://docs.m5stack.com/en/core/ATOM%20Lite)
- USB power source
- USB cable
- Kenwood TH-D74 or TH-D75
- iPhone or iPad with
  [B.B. Link Configurator](https://apps.apple.com/us/app/b-b-link-configurator/id6476163710) and [RadioMail](https://radiomail.app)

The ATOM Lite can be powered from a USB adapter, power bank, or a compatible
iPhone/iPad USB connection. The firmware uses an 80 MHz CPU clock, reduced
Bluetooth transmit power, and low RGB LED brightness to reduce power draw.
> ![INFORMATION] Alternative Power Source
> [ATOMIC Motion Base v1.2 with Power Monitor (INA226AIDGSR)
SKU: A090-V12](https://shop.m5stack.com/products/atomic-motion-base-v1-2-with-power-monitor)

## Build and upload

Firmware installation is performed locally over USB with Arduino IDE or
Arduino CLI. This repository does not include a browser flasher or an automated
build and release workflow.

### Arduino IDE

1. Install the latest [Arduino IDE](https://www.arduino.cc/en/software).
2. [Setup M5Stack Environment](https://docs.m5stack.com/en/arduino/arduino_board)
3. Install these libraries in Library Manager:

   | Library | Version |
   |---|---:|
   | M5Unified | 0.2.19 |
   | FastLED | 3.10.5 |
   | ArduinoQueue | 1.2.5 |

5. Open `bb-link.ino`.
6. Select **M5Atom** from **Tools > Board**. This matches M5Stack's
   [official ATOM Lite upload guide](https://docs.m5stack.com/en/arduino/m5atom/program).
7. Select the ATOM Lite serial port and click **Upload**.

## Initial setup

### Pair the radio

1. Power on the ATOM Lite.
2. Open B.B. Link Configurator and select `B.B. Link`.
3. On the radio, open **Menu > Bluetooth > Pairing Mode**.
4. In the Configurator, open **Paired Radio** and select the radio.
5. Accept the pairing request on the radio.

The pairing is stored and the adapter reconnects automatically after restart.
A breathing blue LED indicates that the saved radio is connected.

### Connect RadioMail

1. Fully close B.B. Link Configurator.
2. In RadioMail, open **Settings > KISS TNC Modem > Default TNC**.
3. Select `B.B. Link`, then tap **Done**.
4. Open the connection screen and select a packet station.

A solid blue LED indicates that both the radio and the iOS device are connected.

## Usage

### Controls

| Action | Result |
|---|---|
| Short press | Disconnect and retry the saved radio connection |
| Hold for 2 seconds while running | Enter deep sleep |
| Press while sleeping | Wake the adapter |

### LED status

| LED | State |
|---|---|
| 🟡 | Waiting to pair |
| 🔵 Slow flashing | Scanning for the radio |
| 🔵 Breathing | Radio connected; waiting for iOS |
| 🔵 Solid | Radio and iOS connected |
| 🟡 Fast flashing | Entering deep sleep |
| 🔴 Slow flashing | Fatal error; reset required |
| 🟢 Activity | Receiving from the radio |
| 🔴 Activity | Transmitting to the radio |
| 🟣 Activity | Simultaneous receive/transmit activity |

### Rig control

Rig control is enabled by default. The adapter initializes KISS mode and can
respond to RadioMail frequency-change commands. Disable **Control Frequency**
in B.B. Link Configurator if the adapter must not change radio settings.

### Factory reset

Use **Reset Adapter** in B.B. Link Configurator, or connect a serial monitor at
115200 baud and send an uppercase `R`. This clears saved configuration and
Bluetooth pairing information.

## Troubleshooting

If the adapter connects but the radio does not transmit, open
**Menu > Configuration > Interface > KISS (983)** on the radio and set KISS to
**Bluetooth**.

## Repository layout

```text
bb-link.ino        Arduino sketch entry point
src/               Firmware implementation and headers
LICENSE            GPL-3.0 license
README.md          Build and operating instructions
```

The repository root is the Arduino sketch directory, named `bb-link` to match
`bb-link.ino`. Its internal C++ sources are kept in the sketch's `src/`
directory, which Arduino compiles recursively according to the
[official sketch specification](https://docs.arduino.cc/arduino-cli/sketch-specification/#src-subfolder).

## License and upstream project

This project is licensed under GPL-3.0. The original B.B. Link project and
product documente were maintained by
[Island Magic Co.](https://islandmagic.co/bb-link), with the upstream source at
[islandmagic/bb-link](https://github.com/islandmagic/bb-link).
