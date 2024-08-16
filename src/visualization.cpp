#include "visualization.h"

#define FASTLED_INTERNAL  // silence FastLED SPI warning
#include <FastLED.h>

#include "macros.h"

static CRGB leds[LED_MATRIX_N] = {CRGB::Black};
static Visualization currentVisualization = VISUALIZATION_NONE;
static CRGB color = CRGB::Black;

void setupLedStrip() {
    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN, GRB>(leds, LED_MATRIX_N);
    FastLED.show();
}

void setupVisualization(Visualization visualization) {
    switch(visualization) {
        case VISUALIZATION_RED_BARS:
            color = CRGB::Red;
            break;
        case VISUALIZATION_GREEN_BARS:
            color = CRGB::Green;
            break;
        case VISUALIZATION_BLUE_BARS:
            color = CRGB::Blue;
            break;
    }
}

void teardownVisualization(Visualization visualization) {
    for (int i= 0; i < LED_MATRIX_N; i++) leds[i] = CRGB::Black;
    currentVisualization = VISUALIZATION_NONE;
    color = CRGB::Black;
}

void updateVisualization(float *bars) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        int offset = i > 0 ? -4 : 0;
        int barLength = i > 0 ? LED_MATRIX_N_PER_BAND : LED_MATRIX_N_PER_BAND - 4;
        int toLight = int(bars[i] * LED_MATRIX_N_PER_BAND);
        if (toLight > barLength) toLight = barLength;
        toLight = toLight > LED_MATRIX_N_PER_BAND ? LED_MATRIX_N_PER_BAND : toLight;
        int toSkip = barLength - toLight;

        if (i % 2 == 0) {
            for (int j = 0; j < toLight; j++) {
                leds[offset + i * LED_MATRIX_N_PER_BAND + j] = color;
            }
            for (int j = 0; j < toSkip; j++) {
                leds[offset + i * LED_MATRIX_N_PER_BAND + j + toLight] = CRGB::Black;
            }
        } else {
            for (int j = 0; j < toSkip; j++) {
                leds[offset + i * LED_MATRIX_N_PER_BAND + j] = CRGB::Black;
            }
            for (int j = 0; j < toLight; j++) {
                leds[offset + i * LED_MATRIX_N_PER_BAND + j + toSkip] = color;
            }
        }
    }
    FastLED.show();
}
