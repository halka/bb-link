#include <esp_sleep.h>
#include "Adapter.h"

#define ACTION_FEEDBACK_DURATION 2000    // Duration for action feedback (long-press registered)
#define LINGER_TIME_BEFORE_SHUTDOWN 2000 // Grace time so user can release button before sleep

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
  statusIndicator.init();
  button.init();

  button.setOnLongPressed([this]() { onLongPressed(); });
  button.setOnShortPressed([this]() { onShortPressed(); });

  const bool bridgeReady = bridge.init();
  if (!bridgeReady) {
    statusIndicator.set(error);
    while (true) {
      statusIndicator.render();
      delay(10);
    }
  }

  bridge.perform();
}

void Adapter::perform() {
  statusIndicator.render();
  button.process();
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
  bridge.disconnect();
  statusIndicator.sleep();

  // Wake by pressing the Atom Lite button (GPIO39 pulled LOW when pressed).
  esp_sleep_enable_ext0_wakeup(ATOM_LITE_BTN_GPIO, 0);
  esp_deep_sleep_start();
}

void Adapter::onLongPressed() {
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
  bridge.reconnectRadio();
}

void Adapter::idleEnter() {
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
  statusIndicator.set(shutdown);
}

void Adapter::shutdownUpdate() {
  if (adapterStateMachine.timeInCurrentState() > LINGER_TIME_BEFORE_SHUTDOWN) {
    doShutdown();
  }
}

void Adapter::shutdownExit() {
}

