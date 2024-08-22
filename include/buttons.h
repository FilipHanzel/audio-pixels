#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

/**
 * @brief Represents the state of a button for use in a debouncing routine.
 */
typedef struct {
    byte stableState = HIGH;            // Last stable state of the button
    byte lastState = HIGH;              // Last observed state of the button
    unsigned long lastDebounceTime = 0; // Last time the button state was changed
} ButtonDebounceState;

/**
 * @brief Debounces the button press and detects a release event.
 *
 * This function checks the current reading of a button, updates the debouncing
 * state, and determines whether the button was released. It helps to avoid
 * false readings due to button bounce.
 *
 * @param bds Pointer to the `ButtonDebounceState` struct tracking the button's state.
 * @param reading Current, unprocessed reading of the button's state.
 *
 * @return `true` if button was released, `false` otherwise.
 */
bool debouncedRelease(ButtonDebounceState *bds, int reading);

#endif
