/*
  (c) 2024 Island Magic Co. All Rights Reserved.
  (c) 2025-2026 JM8UTW

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

#include "src/Adapter.h"
Adapter* adapter = nullptr;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  // Slow down to save power. ESP32 BT Classic + BLE coexistence works at 80 MHz.
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  delay(1000);

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

  const uint32_t heapSize = ESP.getHeapSize();
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t sketchSize = ESP.getSketchSize();
  const uint32_t freeSketchSpace = ESP.getFreeSketchSpace();
  const uint32_t sketchCapacity = sketchSize + freeSketchSpace;

  Serial.printf("Heap free: %d, usage: %d %%\n", freeHeap, 100 - (freeHeap * 100) / heapSize);
  Serial.printf("Flash size: %d, total: %d, usage: %d %%\n", sketchSize, sketchCapacity, (sketchSize * 100) / sketchCapacity);
  Serial.printf("CPU clock: %d Mhz\n", getCpuFrequencyMhz());
  adapter->init();
}

void loop() {
  M5.update();
  adapter->perform();
}
