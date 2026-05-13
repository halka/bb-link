#pragma once
#ifndef M5BUTTON_H
#define M5BUTTON_H

#include <M5Unified.h>
#include "ButtonBase.h"

#define LONG_PRESS_MS 2000

class M5Button : public ButtonBase
{
public:
    M5Button() = default;

    void init() override
    {
        // M5.begin() in setup() already initializes BtnA on the Atom Lite.
        longPressFired = false;
    }

    void process() override
    {
        // M5.update() must have been called in the main loop before this.

        if (M5.BtnA.isPressed())
        {
            if (!longPressFired && M5.BtnA.pressedFor(LONG_PRESS_MS))
            {
                longPressFired = true;
                if (onLongPressed)
                {
                    onLongPressed();
                }
            }
            return;
        }

        if (M5.BtnA.wasReleased())
        {
            if (longPressFired)
            {
                // Long press already consumed the event.
                longPressFired = false;
                return;
            }
            if (onShortPressed)
            {
                onShortPressed();
            }
        }
    }

    void setOnShortPressed(std::function<void(void)> cb) override
    {
        onShortPressed = cb;
    }

    void setOnLongPressed(std::function<void(void)> cb) override
    {
        onLongPressed = cb;
    }

private:
    std::function<void(void)> onShortPressed = nullptr;
    std::function<void(void)> onLongPressed = nullptr;
    bool longPressFired = false;
};

#endif // M5BUTTON_H
