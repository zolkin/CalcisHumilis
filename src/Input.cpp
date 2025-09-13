#include "Input.h"

void Input::poll()
{
    const uint8_t now = digitalRead(pins_.trigBtn);
    const uint32_t t = millis();

    switch (state_)
    {
    case IDLE:
        // first transition to active level?
        if (now == activeLevel_)
        {
            state_ = PRECONFIRM;
            tStart_ = t;
        }
        break;

    case PRECONFIRM:
        if (now != activeLevel_)
        {
            // bounced back → ignore and return to idle
            state_ = IDLE;
        }
        else if ((uint32_t)(t - tStart_) >= preMs_)
        {
            // still pressed after pre-confirm → FIRE once
            trigPressed_ = true;
            state_ = HELD; // lock until release
        }
        break;

    case HELD:
        // stay here as long as the button is held down
        if (now == idleLevel_)
        {
            state_ = RELEASE_CONFIRM;
            tStart_ = t;
        }
        break;

    case RELEASE_CONFIRM:
        if (now == activeLevel_)
        {
            // pressed again during release window → go back to HELD
            state_ = HELD;
        }
        else if ((uint32_t)(t - tStart_) >= releaseMs_)
        {
            // fully released and stable → re-arm
            state_ = IDLE;
        }
        break;
    }
}