# B.B. Link Atom Operation Guide

## Pair the radio

1. Power on the ATOM Lite.
2. Open B.B. Link Configurator and select `B.B. Link`.
3. On the radio, open **Menu > Bluetooth > Pairing Mode**.
4. In the Configurator, open **Paired Radio** and select the radio.
5. Accept the pairing request on the radio.

The pairing is stored, and the adapter reconnects automatically after restart.
A breathing blue LED indicates that the saved radio is connected.

## Connect RadioMail

1. Fully close B.B. Link Configurator.
2. In RadioMail, open **Settings > KISS TNC Modem > Default TNC**.
3. Select `B.B. Link`, then tap **Done**.
4. Open the connection screen and select a packet station.

A solid blue LED indicates that both the radio and the iOS device are connected.

## Controls

| Action | Result |
| --- | --- |
| Short press | Disconnect and retry the saved radio connection (or exit battery charging mode) |
| Press 3+ times | Enter battery charging mode (disables Bluetooth radio emissions) |
| Hold for 2 seconds while running | Enter deep sleep |
| Press while sleeping | Wake the adapter |

## LED status

| LED | State |
| --- | --- |
| 🟡 | Waiting to pair |
| 🔵 Slow flashing | Scanning for the radio |
| 🔵 Breathing | Radio connected; waiting for iOS |
| 🔵 Solid | Radio and iOS connected |
| 🩵 Slow flashing | Battery charging mode (Bluetooth off) |
| 🟡 Fast flashing | Entering deep sleep |
| 🔴 Slow flashing | Fatal error; reset required |
| 🟢 Activity | Receiving from the radio |
| 🔴 Activity | Transmitting to the radio |
| 🟣 Activity | Simultaneous receive/transmit activity |

## Rig control

Rig control is enabled by default. The adapter initialises KISS mode and can
respond to RadioMail frequency-change commands. Disable **Control Frequency**
in B.B. Link Configurator if the adapter must not change radio settings.

## Troubleshooting

If the adapter connects but the radio does not transmit, open
**Menu > Configuration > Interface > KISS (983)** on the radio and set KISS to
**Bluetooth**.

## Factory reset

Use **Reset Adapter** in B.B. Link Configurator

