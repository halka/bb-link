#pragma once
#ifndef STATUS_INDICATOR_M5ATOM_H
#define STATUS_INDICATOR_M5ATOM_H

// Atom Lite has a single SK6812 RGB LED on GPIO27. M5Unified does not drive it,
// so we use FastLED here. Install FastLED via the Arduino Library Manager.
#include <FastLED.h>
#include <math.h>
#include "StatusIndicatorBase.h"

#define ATOM_LITE_LED_GPIO 27
#ifndef STATUS_INDICATOR_DEFAULT_BRIGHTNESS
#define STATUS_INDICATOR_DEFAULT_BRIGHTNESS 32
#endif

#define STATUS_INDICATOR_BOOT_TEST_MS 250

#define FLASH_LED_ON_PERIOD_MS 60
#define FLASH_LED_OFF_PERIOD_MS 600
#define FAST_BLINK_LED_ON_PERIOD_MS 60
#define FAST_BLINK_LED_OFF_PERIOD_MS 40

class StatusIndicator_M5Atom : public StatusIndicatorBase
{
public:
  StatusIndicator_M5Atom() = default;

  void init() override
  {
    FastLED.addLeds<SK6812, ATOM_LITE_LED_GPIO, GRB>(&led, 1);
    FastLED.setBrightness(STATUS_INDICATOR_DEFAULT_BRIGHTNESS);
    led = CRGB(0xff8503);
    FastLED.show();
    delay(STATUS_INDICATOR_BOOT_TEST_MS);
    led = CRGB::Black;
    FastLED.show();

    status = disconnected;
    lastUpdate = millis();
    ledOn = false;
    currentColor = 0;
    currentBrightness = STATUS_INDICATOR_DEFAULT_BRIGHTNESS;
  }

  void set(status_t new_status) override
  {
    status = new_status;
  }

  void render() override
  {
    setColorMaybe(colorForStatus());

    switch (modeForStatus())
    {
    case fixed:
      setBrightnessMaybe(STATUS_INDICATOR_DEFAULT_BRIGHTNESS);
      ledOn = true;
      break;

    case breathe:
    {
      // https://thingpulse.com/breathing-leds-cracking-the-algorithm-behind-our-breathing-pattern/
      float ledBreatheValue = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
      setBrightnessMaybe((uint8_t)ledBreatheValue);
    }
    break;

    case flash:
      tickBlink(FLASH_LED_ON_PERIOD_MS, FLASH_LED_OFF_PERIOD_MS);
      break;

    case fastBlink:
      tickBlink(FAST_BLINK_LED_ON_PERIOD_MS, FAST_BLINK_LED_OFF_PERIOD_MS);
      break;

    case fadeOut:
    {
      unsigned long now = millis();
      if (now - lastUpdate >= 80)
      {
        if (currentBrightness == 0) return;
        setBrightnessMaybe(currentBrightness - 1);
        lastUpdate = now;
      }
    }
    break;
    }
  }

  void sleep() override
  {
    led = CRGB::Black;
    FastLED.show();
  }

private:
  CRGB led;
  status_t status = disconnected;
  bool ledOn = false;
  unsigned long lastUpdate = 0;
  uint32_t currentColor = 0;
  uint8_t currentBrightness = STATUS_INDICATOR_DEFAULT_BRIGHTNESS;

  void setBrightnessMaybe(uint8_t brightness)
  {
    if (currentBrightness != brightness)
    {
      currentBrightness = brightness;
      FastLED.setBrightness(currentBrightness);
    }
    FastLED.show();
  }

  void setColorMaybe(uint32_t color)
  {
    if (currentColor != color)
    {
      currentColor = color;
      led = CRGB(color);
    }
    FastLED.show();
  }

  void tickBlink(unsigned long onPeriod, unsigned long offPeriod)
  {
    unsigned long now = millis();
    unsigned long delta = now - lastUpdate;
    if (ledOn)
    {
      if (delta >= onPeriod)
      {
        setBrightnessMaybe(0);
        ledOn = false;
        lastUpdate = now;
      }
    }
    else
    {
      if (delta >= offPeriod)
      {
        setBrightnessMaybe(STATUS_INDICATOR_DEFAULT_BRIGHTNESS);
        ledOn = true;
        lastUpdate = now;
      }
    }
  }

  uint32_t colorForStatus()
  {
    switch (status)
    {
    case disconnected:      // Idle, waiting to pair
    case shutdown:          // Shutting down
    case actionRegistered:  // Long-press registered
    case otaFlash:          // OTA in progress (amber, fast blink)
      return 0xff8503;      // Amber

    case scanning:          // Scanning for radio
    case connected:         // Paired with radio (breathing)
    case ready:             // Radio + iOS paired
      return 0x0000FF;      // Blue

    case rx:
      return 0x00FF00;      // Green

    case tx:
      return 0xFF0000;      // Red

    case duplex:
      return 0xA020F0;      // Purple

    case error:
    case batteryLow:
    case batteryShutdown:
      return 0xFF0000;      // Red

    case batteryFull:
      return 0x00FF00;
    }
    return 0xFFFFFF;
  }

  led_mode_t modeForStatus()
  {
    switch (status)
    {
    case scanning:          // Blue, slow flash
      return flash;
    case connected:         // Blue, breathing (paired with radio, awaiting iOS)
      return breathe;
    case shutdown:          // Amber, fast blink
    case actionRegistered:
    case batteryShutdown:   // Red, fast blink (low-bat immediate shutdown)
    case otaFlash:
      return fastBlink;
    case error:             // Red, slow flash (fatal error)
      return flash;
    default:
      return fixed;
    }
  }
};

#endif // STATUS_INDICATOR_M5ATOM_H
