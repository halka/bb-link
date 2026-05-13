#pragma once
#ifndef ADAPTER_H
#define ADAPTER_H

#include <Arduino.h>
#include <M5Unified.h>

#include "StatusIndicatorBase.h"
#include "StatusIndicator_M5Atom.h"
#include "ButtonBase.h"
#include "M5Button.h"
#include "Bridge.h"
#include "FiniteStateMachine.h"

#include <esp_ota_ops.h>

#define ADAPTER_NAME "B.B. Link"

#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 7
#define FIRMWARE_VERSION_PATCH 8

#define DEVICE_NAMESPACE "bb-link-hw"
#define IDENTITY_KEY "identity"

// Atom Lite button (GPIO39). RTC-IO capable so it can wake from deep sleep.
#define ATOM_LITE_BTN_GPIO GPIO_NUM_39

enum hardware_board_t {
  hardware_board_unknown = 0,
  hardware_board_m5atom = 3
};

#define HARDWARE_BOARD hardware_board_m5atom
#define HARDWARE_VERSION_MAJOR 1
#define HARDWARE_VERSION_MINOR 0

const char *getHardwareName();

DECLARE_STATE(AdapterState);

enum shutdown_reason_t {
  userInitiated = 0x00,
  idleTimeout = 0x01,
  lowBattery = 0x02
};

class Adapter : public BLECharacteristicCallbacks {
public:
  Adapter();
  void init();
  void perform();
  Bridge bridge;
  String getAdapterName();

private:
  StatusIndicator_M5Atom statusIndicator = StatusIndicator_M5Atom();
  M5Button button = M5Button();

  shutdown_reason_t shutdownReason;

  AdapterState idleState;
  AdapterState inUseState;
  AdapterState shutdownState;
  AdapterState otaFlashState;
  FSMT<AdapterState> adapterStateMachine;

  BLECharacteristic *pOtaFlash;
  BLECharacteristic *pOtaIdentity;
  esp_ota_handle_t otaHandle = 0;

  void verifyFirmware();
  void onLongPressed();
  void onShortPressed();
  void updateSendReceiveStatus();
  void doShutdown();

  void shutdownEnter();
  void shutdownUpdate();
  void shutdownExit();
  void idleEnter();
  void idleUpdate();
  void idleExit();
  void inUseEnter();
  void inUseUpdate();
  void inUseExit();
  void otaFlashEnter();
  void otaFlashUpdate();
  void otaFlashExit();

  void initBLEOtaService();

  void onWrite(BLECharacteristic *pCharacteristic);
  void onRead(BLECharacteristic *pCharacteristic);

  String fetchAdapterName();
};

#endif
