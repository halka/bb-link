#pragma once
#ifndef BUTTONBASE_H
#define BUTTONBASE_H

#include <functional>

class ButtonBase
{
public:
    virtual ~ButtonBase() = default;

    virtual void init() = 0;
    virtual void process() = 0;

    virtual void setOnShortPressed(std::function<void(void)> cb) = 0;
    virtual void setOnLongPressed(std::function<void(void)> cb) = 0;
};

#endif
