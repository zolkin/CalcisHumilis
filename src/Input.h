#pragma once
#include <Arduino.h>

struct InputPins
{
    uint8_t trigBtn = 6; // default GP7
};

class Input
{
public:
    // activeHigh = true  -> pressed = HIGH (pulldown wiring, idle LOW)
    // activeHigh = false -> pressed = LOW  (pullup wiring, idle HIGH)
    explicit Input(const InputPins &pins = {}, bool activeHigh = true)
        : pins_(pins), activeHigh_(activeHigh)
    {
        activeLevel_ = activeHigh_ ? HIGH : LOW;
        idleLevel_ = activeHigh_ ? LOW : HIGH;
        lastLevel_ = idleLevel_;
    }

    // usePullups=true only when wiring button to GND (internal pull-up)
    void begin(bool usePullups = false)
    {
        pinMode(pins_.trigBtn, usePullups ? INPUT_PULLUP : INPUT);
    }

    // Set timings: small pre-confirm, longer lockout (defaults: 3 ms / 8 ms)
    void setDebounce(uint16_t preConfirmMs, uint16_t releaseMs)
    {
        preMs_ = preConfirmMs;
        releaseMs_ = releaseMs;
    }

    // Call often from loop()
    void poll();

    // True exactly once per accepted press
    bool takeTrigPressed()
    {
        const bool v = trigPressed_;
        trigPressed_ = false;
        return v;
    }

    // Optional: switch polarity at runtime
    void setActiveHigh(bool ah)
    {
        activeHigh_ = ah;
        activeLevel_ = activeHigh_ ? HIGH : LOW;
        idleLevel_ = activeHigh_ ? LOW : HIGH;
        lastLevel_ = idleLevel_;
        state_ = IDLE;
        trigPressed_ = false;
    }

private:
    enum State : uint8_t
    {
        IDLE,
        PRECONFIRM,
        HELD,
        RELEASE_CONFIRM
    };

    InputPins pins_;
    bool activeHigh_ = true;
    uint8_t activeLevel_ = HIGH; // level meaning "pressed"
    uint8_t idleLevel_ = LOW;    // level meaning "not pressed"
    uint8_t lastLevel_ = LOW;

    // timings
    uint16_t preMs_ = 3;     // 2–4 ms feels snappy
    uint16_t releaseMs_ = 8; // 6–10 ms kills release bounce

    // state
    State state_ = IDLE;
    uint32_t tStart_ = 0;
    volatile bool trigPressed_ = false;
};