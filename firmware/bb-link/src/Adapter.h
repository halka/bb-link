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

#ifndef ADAPTER_NAME
#define ADAPTER_NAME "B.B. Link"
#endif

#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 7
#define FIRMWARE_VERSION_PATCH 9

#define DEVICE_NAMESPACE "bb-link-hw"
#define IDENTITY_KEY "identity"

#ifndef BB_LINK_MOBILE_POWER_PROFILE
#define BB_LINK_MOBILE_POWER_PROFILE 1
#endif

// Atom Lite button (GPIO39). RTC-IO capable so it can wake from deep sleep.
#define ATOM_LITE_BTN_GPIO GPIO_NUM_39

#ifndef BB_LINK_ENABLE_BLE_OTA
#define BB_LINK_ENABLE_BLE_OTA 1
#endif

// Unsafe unsigned BLE OTA stays disabled unless a developer explicitly opts
// in. Production builds should enable signed-app verification in ESP-IDF.
#ifndef BB_LINK_ALLOW_UNSIGNED_OTA_WITH_PHYSICAL_ACCESS
#define BB_LINK_ALLOW_UNSIGNED_OTA_WITH_PHYSICAL_ACCESS 0
#endif

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

  BLECharacteristic *pOtaFlash = nullptr;
  BLECharacteristic *pOtaIdentity = nullptr;
  esp_ota_handle_t otaHandle = 0;
  const esp_partition_t *otaPartition = nullptr;
  size_t otaBytesWritten = 0;
  bool otaModeEnabled = false;
  bool otaWriteInProgress = false;
  unsigned long otaModeStartedAt = 0;

  void verifyFirmware(bool selfTestPassed);
  bool otaModeRequested();
  bool otaSecurityConfigured();
  void abortOta(const char *reason, esp_err_t error = ESP_FAIL);
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

  bool initBLEOtaService();

  void onWrite(BLECharacteristic *pCharacteristic);
  void onRead(BLECharacteristic *pCharacteristic);

  String fetchAdapterName();
};

#endif
