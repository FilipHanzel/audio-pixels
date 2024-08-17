#include "visualization.h"

#define FASTLED_INTERNAL // silence FastLED SPI warning
#include <FastLED.h>

#include "macros.h"

typedef struct {
    CRGB color;
} VisStateColorBars;

typedef struct {
    CRGBPalette16 palette;
} VisStateSpectrum;

DEFINE_GRADIENT_PALETTE(blank_gp){
    0, 0, 0, 0,
    255, 0, 0, 0
};

DEFINE_GRADIENT_PALETTE(red_gp){
    0, 0, 0, 0,
    128, 255, 0, 0,
    255, 255, 200, 200
};

DEFINE_GRADIENT_PALETTE(green_gp){
    0, 0, 0, 0,
    128, 0, 255, 0,
    255, 200, 255, 200
};

DEFINE_GRADIENT_PALETTE(blue_gp){
    0, 0, 0, 0,
    128, 0, 0, 255,
    255, 200, 200, 255
};

DEFINE_GRADIENT_PALETTE(heatmapGreen_gp){
    0, 0, 5, 0,
    64, 0, 150, 0,
    128, 255, 0, 0,
    192, 255, 85, 0,
    255, 255, 255, 50
};

DEFINE_GRADIENT_PALETTE(heatmapBlue_gp){
    0, 0, 0, 5,
    64, 0, 0, 200,
    128, 255, 0, 0,
    192, 255, 85, 0,
    255, 255, 255, 50
};

DEFINE_GRADIENT_PALETTE(magmaPink_gp){
    0, 0, 0, 4,
    30, 28, 16, 68,
    60, 79, 18, 123,
    90, 129, 37, 129,
    120, 181, 54, 103,
    150, 229, 80, 63,
    180, 252, 136, 37,
    210, 251, 194, 71,
    240, 254, 253, 191,
    255, 255, 255, 255
};

const static CRGBPalette16 blankPalette = blank_gp;
const static CRGBPalette16 redPalette = red_gp;
const static CRGBPalette16 greenPalette = green_gp;
const static CRGBPalette16 bluePalette = blue_gp;
const static CRGBPalette16 heatmapGreenPalette = heatmapGreen_gp;
const static CRGBPalette16 heatmapBluePalette = heatmapBlue_gp;
const static CRGBPalette16 magmaPinkPalette = magmaPink_gp;

static CRGB leds[LED_MATRIX_N] = {CRGB::Black};
static VisualizationType currentVisualization = VISUALIZATION_TYPE_NONE;
static CRGBPalette16 currentPalette = blankPalette;

void setupLedStrip() {
    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN, GRB>(leds, LED_MATRIX_N);
    FastLED.show();
}

void setupVisualization(VisualizationType visualization) {
    if (currentVisualization != VISUALIZATION_TYPE_NONE) {
        PRINTF("Visualization is already set up. Halt!\n");
        while (true) continue;
    }
    currentVisualization = visualization;
}

void setVisualizationPalette(VisualizationPalette palette) {
    if (currentVisualization == VISUALIZATION_TYPE_NONE) {
        PRINTF("Visualization is not set up. Halt!\n");
        while (true) continue;
    }
    switch (currentVisualization) {
        case VISUALIZATION_TYPE_BARS:
            switch (palette) {
                case VISUALIZATION_PALETTE_BARS_RED:
                    currentPalette = redPalette;
                    break;
                case VISUALIZATION_PALETTE_BARS_GREEN:
                    currentPalette = greenPalette;
                    break;
                case VISUALIZATION_PALETTE_BARS_BLUE:
                    currentPalette = bluePalette;
                    break;
            }
            break;
        case VISUALIZATION_TYPE_SPECTRUM:
            switch (palette) {
                case VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_GREEN:
                    currentPalette = heatmapGreenPalette;
                    break;
                case VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_BLUE:
                    currentPalette = heatmapBluePalette;
                    break;
                case VISUALIZATION_PALETTE_SPECTRUM_MAGMA_PINK:
                    currentPalette = magmaPinkPalette;
                    break;
            }
            break;
    }
}

void teardownVisualization(VisualizationType visualization) {
    if (currentVisualization == VISUALIZATION_TYPE_NONE) {
        PRINTF("Visualization is not set up. Halt!\n");
        while (true) continue;
    }
    currentVisualization = VISUALIZATION_TYPE_NONE;

    currentPalette = blankPalette;
    for (int i = 0; i < LED_MATRIX_N; i++) {
        leds[i] = CRGB::Black;
    }
}

static void updateColorBars(float *bars) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        int toLight = int(bars[i] * LED_MATRIX_N_PER_BAND);
        if (toLight > LED_MATRIX_N_PER_BAND) {
            toLight = LED_MATRIX_N_PER_BAND;
        }
        int toSkip = LED_MATRIX_N_PER_BAND - toLight;

        if (i % 2 == 0) {
            for (int j = 0; j < toLight; j++) {
                leds[i * LED_MATRIX_N_PER_BAND + j] = ColorFromPalette(currentPalette, 128);
            }
            for (int j = 0; j < toSkip; j++) {
                leds[i * LED_MATRIX_N_PER_BAND + j + toLight] = ColorFromPalette(currentPalette, 1);
            }
        } else {
            for (int j = 0; j < toSkip; j++) {
                leds[i * LED_MATRIX_N_PER_BAND + j] = ColorFromPalette(currentPalette, 1);
            }
            for (int j = 0; j < toLight; j++) {
                leds[i * LED_MATRIX_N_PER_BAND + j + toSkip] = ColorFromPalette(currentPalette, 128);
            }
        }
    }
}

static void updateSpectrum(float *bars) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float bar = bars[i];
        if (bar > 1.0) {
            bar = 1.0;
        }
        uint8_t colorIndex = int(bar * 255.0);

        if (i % 2 == 0) {
            for (int j = LED_MATRIX_N_PER_BAND - 1; j > 0; j--) {
                leds[i * LED_MATRIX_N_PER_BAND + j] = leds[i * LED_MATRIX_N_PER_BAND + j - 1];
            }

            leds[i * LED_MATRIX_N_PER_BAND] = ColorFromPalette(currentPalette, colorIndex);
        } else {
            for (int j = 0; j < LED_MATRIX_N_PER_BAND - 1; j++) {
                leds[i * LED_MATRIX_N_PER_BAND + j] = leds[i * LED_MATRIX_N_PER_BAND + j + 1];
            }

            leds[i * LED_MATRIX_N_PER_BAND + LED_MATRIX_N_PER_BAND - 1] = ColorFromPalette(currentPalette, colorIndex);
        }
    }
}

void updateVisualization(float *bars) {
    switch (currentVisualization) {
        case VISUALIZATION_TYPE_BARS:
            updateColorBars(bars);
            break;
        case VISUALIZATION_TYPE_SPECTRUM:
            updateSpectrum(bars);
            break;
    }
    FastLED.show();
}
