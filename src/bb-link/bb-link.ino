/*
  (c) 2024 Island Magic Co. All Rights Reserved.

  M5 ATOM LITE port (M5Unified). External power assumed.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#include <M5Unified.h>
#include <ArduinoLog.h>

const char* logLevels[] = { "OFF", "FATAL", "ERROR", "WARNING", "INFO", "TRACE", "VERBOSE" };

#include "Adapter.h"
Adapter* adapter = nullptr;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  // Slow down to save power. ESP32 BT Classic + BLE coexistence works at 80 MHz.
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  delay(1000);

  Log.begin(LOG_LEVEL_INFO, &Serial);
  Log.setPrefix(logPrintPrefix);

  adapter = new Adapter();

  Serial.println("\n ___   ___     _    _      _");
  Serial.println("| _ ) | _ )   | |  (_)_ _ | |__");
  Serial.println("| _ \\_| _ \\_  | |__| | ' \\| / /");
  Serial.println("|___(_)___(_) |____|_|_||_|_\\_\\\n");

  Serial.printf("Booting up %s v%d.%d.%d on %s v%d.%d\n\n",
                adapter->getAdapterName().c_str(),
                FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH,
                getHardwareName(),
                HARDWARE_VERSION_MAJOR, HARDWARE_VERSION_MINOR);

  Serial.printf("Heap free: %d, usage: %d %%\n", ESP.getFreeHeap(), 100 - (ESP.getFreeHeap() * 100) / ESP.getHeapSize());
  Serial.printf("Flash size: %d, total: %d, usage: %d %%\n", ESP.getSketchSize(), ESP.getFreeSketchSpace(), (ESP.getSketchSize() * 100) / ESP.getFreeSketchSpace());
  Serial.printf("CPU clock: %d Mhz\n", getCpuFrequencyMhz());
  Serial.printf("Log level: %s\n", logLevels[Log.getLevel()]);

  adapter->init();
}

void loop() {
  M5.update();
  adapter->perform();
}

void logPrintPrefix(Print* _logOutput, int logLevel) {
  logPrintSetColor(_logOutput, logLevel);
  logPrintTimestamp(_logOutput);
}

void logPrintSetColor(Print* _logOutput, int logLevel) {
  switch (logLevel) {
    case LOG_LEVEL_FATAL:
    case LOG_LEVEL_ERROR:
      _logOutput->print("\033[31m");
      break;
    case LOG_LEVEL_WARNING:
      _logOutput->print("\033[33m");
      break;
    case LOG_LEVEL_INFO:
      _logOutput->print("\033[0m");
      break;
    case LOG_LEVEL_TRACE:
      _logOutput->print("\033[36m");
      break;
  }
}

void logPrintTimestamp(Print* _logOutput) {
  const unsigned long MSECS_PER_SEC = 1000;
  const unsigned long SECS_PER_MIN = 60;
  const unsigned long SECS_PER_HOUR = 3600;
  const unsigned long SECS_PER_DAY = 86400;
  const unsigned long msecs = millis();
  const unsigned long secs = msecs / MSECS_PER_SEC;
  const unsigned long milliseconds = msecs % MSECS_PER_SEC;
  const unsigned long seconds = secs % SECS_PER_MIN;
  const unsigned long minutes = (secs / SECS_PER_MIN) % SECS_PER_MIN;
  const unsigned long hours = (secs % SECS_PER_DAY) / SECS_PER_HOUR;

  char timestamp[20];
  sprintf(timestamp, "%02lu:%02lu:%02lu.%03lu ", hours, minutes, seconds, milliseconds);

  _logOutput->print(timestamp);
}
