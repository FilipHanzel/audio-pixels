#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

/**
 * @brief Button state struct used in debouncing routine.
 */
typedef struct {
    byte stableState = HIGH;
    byte lastState = HIGH;
    unsigned long lastDebounceTime = 0;
} ButtonDebounceState;

/**
 * @brief Button debouncing routine.
 *
 * @param bds Pointer to the `ButtonDebounceState` struct for the button.
 * @param reading Unprocessed reading of the button state.
 *
 * @return `true` if button was released, `false` otherwise.
 */
bool debouncedRelease(ButtonDebounceState *bds, int reading);

#endif
