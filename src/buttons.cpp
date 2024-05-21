#include "buttons.h"

#define BUTTON_DEBOUNCE_DELAY 20
#define TIME_PASSED_SINCE(T) (millis() - T)

bool debouncedRelease(ButtonDebounceState *bds, int reading) {
    bool is_release = false;

    if (reading != bds->lastState) {
        bds->lastDebounceTime = millis();
    }

    if (TIME_PASSED_SINCE(bds->lastDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
        if (reading != bds->stableState) {
            bds->stableState = reading;

            if (bds->stableState == HIGH) {
                is_release = true;
            }
        }
    }
    bds->lastState = reading;

    return is_release;
}
