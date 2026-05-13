#include <ArduinoLog.h>
#include <esp_sleep.h>
#include "Adapter.h"

#define ACTION_FEEDBACK_DURATION 2000    // Duration for action feedback (long-press registered)
#define LINGER_TIME_BEFORE_SHUTDOWN 2000 // Grace time so user can release button before sleep

#define SERVICE_UUID_OTA "1A68D2B0-C2E4-453F-A2BB-B659D66CF442"
#define CHARACTERISTIC_UUID_OTA_FLASH "1A68D2B1-C2E4-453F-A2BB-B659D66CF442"
#define CHARACTERISTIC_UUID_OTA_IDENTITY "1A68D2B2-C2E4-453F-A2BB-B659D66CF442"

extern const char *logLevels[];

Adapter::Adapter()
  : idleState(
      [this] { this->idleEnter(); },
      [this] { this->idleUpdate(); },
      [this] { this->idleExit(); }),
    inUseState(
      [this] { this->inUseEnter(); },
      [this] { this->inUseUpdate(); },
      [this] { this->inUseExit(); }),
    shutdownState(
      [this] { this->shutdownEnter(); },
      [this] { this->shutdownUpdate(); },
      [this] { this->shutdownExit(); }),
    otaFlashState(
      [this] { this->otaFlashEnter(); },
      [this] { this->otaFlashUpdate(); },
      [this] { this->otaFlashExit(); }),
    adapterStateMachine(idleState),
    bridge(fetchAdapterName()) {
}

String Adapter::fetchAdapterName() {
  String name = ADAPTER_NAME;
  Preferences preferences;

  if (preferences.begin(DEVICE_NAMESPACE, true)) {
    name = preferences.getString(IDENTITY_KEY, ADAPTER_NAME);
    preferences.end();
  }
  return name;
}

const char *getHardwareName() {
  return "M5AtomLite";
}

String Adapter::getAdapterName() {
  return bridge.getAdapterName();
}

void Adapter::init() {
  Log.traceln("Adapter: init");

  statusIndicator.init();
  button.init();

  button.setOnLongPressed([this]() { onLongPressed(); });
  button.setOnShortPressed([this]() { onShortPressed(); });

  if (!bridge.init()) {
    Log.fatalln("FATAL: Bridge init failed !!!!!");
    statusIndicator.set(error);
    while (true) {
      statusIndicator.render();
      delay(10);
    }
  }

  initBLEOtaService();
  verifyFirmware();
}

void Adapter::verifyFirmware() {
  Log.traceln("Checking firmware...");
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    const char *otaState = ota_state == ESP_OTA_IMG_NEW              ? "ESP_OTA_IMG_NEW"
                           : ota_state == ESP_OTA_IMG_PENDING_VERIFY ? "ESP_OTA_IMG_PENDING_VERIFY"
                           : ota_state == ESP_OTA_IMG_VALID          ? "ESP_OTA_IMG_VALID"
                           : ota_state == ESP_OTA_IMG_INVALID        ? "ESP_OTA_IMG_INVALID"
                           : ota_state == ESP_OTA_IMG_ABORTED        ? "ESP_OTA_IMG_ABORTED"
                                                                     : "ESP_OTA_IMG_UNDEFINED";
    Log.infoln("OTA state: %s", otaState);

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        Log.infoln("App is valid, rollback cancelled successfully");
      } else {
        Log.errorln("Failed to cancel rollback");
      }
    }
  } else {
    Log.warningln("OTA partition has no record in OTA data");
  }
}

void Adapter::initBLEOtaService() {
  Log.traceln("Adapter: init BLE OTA service");

  BLEService *pOtaService = bridge.getBLEServer()->createService(SERVICE_UUID_OTA);

  pOtaFlash = pOtaService->createCharacteristic(
    CHARACTERISTIC_UUID_OTA_FLASH,
    BLECharacteristic::PROPERTY_WRITE);

  pOtaIdentity = pOtaService->createCharacteristic(
    CHARACTERISTIC_UUID_OTA_IDENTITY,
    BLECharacteristic::PROPERTY_READ);

  pOtaFlash->addDescriptor(new BLE2902());

  pOtaFlash->setAccessPermissions(ESP_GATT_PERM_WRITE);
  pOtaIdentity->setAccessPermissions(ESP_GATT_PERM_READ);

  pOtaFlash->setCallbacks(this);

  uint8_t identity[6] = { HARDWARE_BOARD, HARDWARE_VERSION_MAJOR, HARDWARE_VERSION_MINOR, FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH };
  pOtaIdentity->setValue(identity, 6);

  pOtaService->start();
}

void Adapter::perform() {
  statusIndicator.render();
  if (!adapterStateMachine.isInState(otaFlashState)) {
    button.process();
  }
  adapterStateMachine.update();
}

void Adapter::updateSendReceiveStatus() {
  if (bridge.isTx() && bridge.isRx()) {
    statusIndicator.set(duplex);
  } else if (bridge.isRx()) {
    statusIndicator.set(rx);
  } else if (bridge.isTx()) {
    statusIndicator.set(tx);
  } else {
    statusIndicator.set(ready);
  }
}

void Adapter::doShutdown() {
  Log.infoln("Going to deep sleep...");
  bridge.disconnect();
  statusIndicator.sleep();

  // Wake by pressing the Atom Lite button (GPIO39 pulled LOW when pressed).
  esp_sleep_enable_ext0_wakeup(ATOM_LITE_BTN_GPIO, 0);
  esp_deep_sleep_start();
}

void Adapter::onLongPressed() {
  Log.infoln("Long pressed button, shutdown");
  shutdownReason = userInitiated;
  statusIndicator.set(actionRegistered);
  unsigned long now = millis();
  while (millis() - now < ACTION_FEEDBACK_DURATION) {
    statusIndicator.render();
    delay(10);
  }
  adapterStateMachine.transitionTo(shutdownState);
}

void Adapter::onShortPressed() {
  Log.infoln("Short pressed button");
  // External power: nothing meaningful to do while awake.
  // Wake-from-deep-sleep is handled by the ext0 wakeup pin.
}

void Adapter::onWrite(BLECharacteristic *pCharacteristic) {
  String rxData = pCharacteristic->getValue();

  if (!adapterStateMachine.isInState(otaFlashState)) {
    Log.infoln("OTA: begin flash");
    adapterStateMachine.immediateTransitionTo(otaFlashState);
    esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &otaHandle);
  }

  if (rxData.length() > 0) {
    esp_ota_write(otaHandle, rxData.c_str(), rxData.length());
    Log.infoln("OTA: written %i bytes", rxData.length());
  } else {
    Log.infoln("OTA: end flash");
    esp_ota_end(otaHandle);
    if (esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL)) == ESP_OK) {
      Log.infoln("OTA: success, rebooting");
      delay(2000);
      esp_restart();
    } else {
      Log.errorln("OTA: failed");
    }
    adapterStateMachine.transitionTo(idleState);
  }
}

void Adapter::onRead(BLECharacteristic *pCharacteristic) {
  Log.errorln("OTA: onRead!!!!");
}

void Adapter::idleEnter() {
  Log.traceln("Adapter: idle");
  statusIndicator.set(disconnected);
}

void Adapter::idleUpdate() {
  bridge.perform();

  if (bridge.isReady()) {
    adapterStateMachine.transitionTo(inUseState);
  } else if (bridge.btcConnected()) {
    statusIndicator.set(connected);
  } else if (bridge.btcDiscovery()) {
    statusIndicator.set(scanning);
  } else {
    statusIndicator.set(disconnected);
  }

  if (Serial.available()) {
    char ch = Serial.read();
    switch (ch) {
      case 'r':
        Serial.println("Rebooting...");
        delay(2000);
        esp_restart();
        break;
      case 'R':
        Serial.println("Perform factory reset");
        bridge.factoryReset();
        adapterStateMachine.transitionTo(idleState);
        break;
      case 'v':
        if (Log.getLevel() > LOG_LEVEL_SILENT) {
          Log.setLevel(Log.getLevel() - 1);
        }
        Serial.printf("Log level: %s\n", logLevels[Log.getLevel()]);
        break;
      case 'V':
        if (Log.getLevel() < LOG_LEVEL_VERBOSE) {
          Log.setLevel(Log.getLevel() + 1);
        }
        Serial.printf("Log level: %s\n", logLevels[Log.getLevel()]);
        break;
      case 'i':
        Serial.printf("Identity: %s\n", getAdapterName().c_str());
        break;
      case 'I':
        char buffer[32];
        Preferences preferences;
        Serial.setTimeout(5000);
        int count = 0;
        count = Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
        if (count == 0) {
          Serial.println("No identity provided");
          if (preferences.begin(DEVICE_NAMESPACE, false)) {
            preferences.remove(IDENTITY_KEY);
            preferences.end();
            Serial.println("Identity removed, rebooting...");
            delay(2000);
            esp_restart();
          } else {
            Serial.println("Failed to open preferences");
          }
        } else {
          Serial.printf("Read %d characters\n", count);
          buffer[count] = '\0';
          Serial.printf("New identity: %s\n", buffer);
          if (preferences.begin(DEVICE_NAMESPACE, false)) {
            int result = preferences.putString(IDENTITY_KEY, buffer);
            preferences.end();
            if (result) {
              Serial.println("Saved, rebooting...");
              delay(2000);
              esp_restart();
            } else {
              Serial.println("Failed to save identity");
            }
          } else {
            Serial.println("Failed to open preferences");
          }
        }
        break;
    }
  }
}

void Adapter::idleExit() {
}

void Adapter::inUseEnter() {
  Log.traceln("Adapter: ready for use");
  statusIndicator.set(ready);
}

void Adapter::inUseUpdate() {
  bridge.perform();

  if (!bridge.isReady()) {
    adapterStateMachine.transitionTo(idleState);
  } else {
    updateSendReceiveStatus();
  }
}

void Adapter::inUseExit() {
}

void Adapter::shutdownEnter() {
  Log.traceln("Adapter: shutdown");
  statusIndicator.set(shutdown);
}

void Adapter::shutdownUpdate() {
  if (adapterStateMachine.timeInCurrentState() > LINGER_TIME_BEFORE_SHUTDOWN) {
    doShutdown();
  }
}

void Adapter::shutdownExit() {
}

void Adapter::otaFlashEnter() {
  Log.traceln("Adapter: OTA flash");
  statusIndicator.set(otaFlash);
  bridge.disconnect();
}

void Adapter::otaFlashUpdate() {
  // Handled by BLECharacteristicCallbacks
}

void Adapter::otaFlashExit() {
}
