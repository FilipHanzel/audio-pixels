#include "visualization.h"

#define FASTLED_INTERNAL // silence FastLED SPI warning
#include <FastLED.h>

#include "macros.h"

// clang-format off

DEFINE_GRADIENT_PALETTE(blank_gp){
      0, 0, 0, 0,
    255, 0, 0, 0
};

DEFINE_GRADIENT_PALETTE(warm_gp){
      0,   0,   0,  0,
     70, 100,   0,  0,
    100, 150,   0,  0,
    150, 150,  40,  0,
    255, 255, 110, 60
};

DEFINE_GRADIENT_PALETTE(ocean_gp){
      0,   0,   0,   0,
     70,   0, 100,   0,
    100,   0, 150,  10,
    150,  10, 150,  40,
    255, 110, 255, 220
};

DEFINE_GRADIENT_PALETTE(funky_gp){
      0,   0,   0,   0,
     90, 150,   0,  50,
    100,  50,   0, 250,
    110,  10,  10, 100,
    255, 200, 200,   0
};

DEFINE_GRADIENT_PALETTE(heatmapRed_gp){
      0,   0,   0,   0,
     70, 100,   0,   0,
    100, 150,   0,   0,
    150, 150,  40,   0,
    255, 255, 110, 100
};

DEFINE_GRADIENT_PALETTE(heatmapGreen_gp){
      0,   0,   1,  0,
     70,   0,  50,  0,
    120,  75, 100,  0,
    140, 100,  75,  0,
    255, 150,   0,  0
};

DEFINE_GRADIENT_PALETTE(heatmapBlue_gp){
      0,   0,  0,   1,
     70,   0,  0,  30,
    120,   0,  0, 100,
    140,  50, 50,  70,
    220, 150,  0,   0,
    255, 170,  0,   0
};

DEFINE_GRADIENT_PALETTE(heatmapPink_gp){
      0,   0,   0,   4,
     30,  14,   8,  34,
     60,  39,   9,  64,
     90,  67,  14,  65,
    130,  80,  25,  50,
    170, 140,  65,  41,
    195, 165, 106,  27,
    230, 198, 150,  55,
    240, 199, 198, 150,
    255, 200, 200, 200
};

DEFINE_GRADIENT_PALETTE(fireRed_gp){
      0,   3,   0,   0,
     40,   4,   0,   0,
     55,  40,   0,   0,
     60,  60,  20,   0,
     70,  90,  80,  10,
     90, 150, 150,  10,
    255, 210, 200, 200
};

DEFINE_GRADIENT_PALETTE(fireBlue_gp){
      0,   0,   0,   3,
     40,   0,   0,  10,
     55,  40,   5,   4,
     60,  60,  20,   2,
     90,  60,  30,   0,
    110, 150, 150,  10,
    255, 210, 200, 200
};

DEFINE_GRADIENT_PALETTE(fireGreen_gp){
      0,   0,   2,   0,
     40,   0,   5,   0,
     55,   0,  40,   0,
     60,  45,  60,   0,
     65,  55,  70,   0,
     80, 220, 140,  10,
    255, 210, 200, 200
};

// clang-format on

const static CRGBPalette16 blankPalette = blank_gp;
const static CRGBPalette16 warmPalette = warm_gp;
const static CRGBPalette16 oceanPalette = ocean_gp;
const static CRGBPalette16 funkyPalette = funky_gp;
const static CRGBPalette16 heatmapRedPalette = heatmapRed_gp;
const static CRGBPalette16 heatmapGreenPalette = heatmapGreen_gp;
const static CRGBPalette16 heatmapBluePalette = heatmapBlue_gp;
const static CRGBPalette16 heatmapPinkPalette = heatmapPink_gp;
const static CRGBPalette16 fireRedPalette = fireRed_gp;
const static CRGBPalette16 fireBluePalette = fireBlue_gp;
const static CRGBPalette16 fireGreenPalette = fireGreen_gp;

static VisualizationType currentVisualization = VISUALIZATION_TYPE_NONE;
static CRGBPalette16 currentPalette = blankPalette;

// Two buffers, primary (A) and secondary (B), are required to apply effects like blur.
// If an animation does not require a secondary buffer, it can operate only on the primary buffer.
static uint8_t colorBufferA[LED_MATRIX_N] = {0};     // Primary LED color buffer
static uint8_t colorBufferB[LED_MATRIX_N] = {0};     // Secondary LED color buffer
static uint8_t brightnessBuffer[LED_MATRIX_N] = {0}; //
static CRGB leds[LED_MATRIX_N] = {CRGB::Black};      // Array of LED colors used directly by the FastLED library
static float bandsBuffer[LED_MATRIX_N_BANDS] = {0};  // Internal buffer for bands values that drive the animation

void setupLedStrip() {
    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN_A, GRB>(leds, 0 * LED_MATRIX_N_PER_DATA_PIN, LED_MATRIX_N_PER_DATA_PIN);
    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN_B, GRB>(leds, 1 * LED_MATRIX_N_PER_DATA_PIN, LED_MATRIX_N_PER_DATA_PIN);
    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN_C, GRB>(leds, 2 * LED_MATRIX_N_PER_DATA_PIN, LED_MATRIX_N_PER_DATA_PIN);
    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN_D, GRB>(leds, 3 * LED_MATRIX_N_PER_DATA_PIN, LED_MATRIX_N_PER_DATA_PIN);
    FastLED.show();
}

void setupVisualization(VisualizationType visualization) {
    if (currentVisualization != VISUALIZATION_TYPE_NONE) {
        PRINTF("Visualization is already set up. Halt!\n");
        while (true) continue;
    }
    currentVisualization = visualization;

    for (int i = 0; i < LED_MATRIX_N; i++) {
        brightnessBuffer[i] = 255;
    }
}

void setVisualizationPalette(VisualizationPalette palette) {
    if (currentVisualization == VISUALIZATION_TYPE_NONE) {
        PRINTF("Visualization is not set up. Halt!\n");
        while (true) continue;
    }
    switch (currentVisualization) {
        case VISUALIZATION_TYPE_BARS:
            switch (palette) {
                case VISUALIZATION_PALETTE_BARS_WARM:
                    currentPalette = warmPalette;
                    break;
                case VISUALIZATION_PALETTE_BARS_OCEAN:
                    currentPalette = oceanPalette;
                    break;
                case VISUALIZATION_PALETTE_BARS_FUNKY:
                    currentPalette = funkyPalette;
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
                case VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_RED:
                    currentPalette = heatmapRedPalette;
                    break;
                case VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_PINK:
                    currentPalette = heatmapPinkPalette;
                    break;
            }
            break;
        case VISUALIZATION_TYPE_FIRE:
            switch (palette) {
                case VISUALIZATION_PALETTE_FIRE_RED:
                    currentPalette = fireRedPalette;
                    break;
                case VISUALIZATION_PALETTE_FIRE_BLUE:
                    currentPalette = fireBluePalette;
                    break;
                case VISUALIZATION_PALETTE_FIRE_GREEN:
                    currentPalette = fireGreenPalette;
                    break;
            }
            break;
    }
}

void teardownVisualization() {
    if (currentVisualization == VISUALIZATION_TYPE_NONE) {
        PRINTF("Visualization is not set up. Halt!\n");
        while (true) continue;
    }
    currentVisualization = VISUALIZATION_TYPE_NONE;
    currentPalette = blankPalette;
    for (int i = 0; i < LED_MATRIX_N; i++) {
        colorBufferA[i] = 0;
        colorBufferB[i] = 0;
        brightnessBuffer[i] = 0;
        leds[i] = CRGB::Black;
    }
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        bandsBuffer[i] = 0;
    }
}

/**
 * @brief Transfers the values from the primary buffer (`colorBufferA`) to the LED array (`leds`).
 */
static void pushBuffer() {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        int offset = i * LED_MATRIX_N_PER_BAND;

        if (i % 2 == 0) {
            for (int j = 0; j < LED_MATRIX_N_PER_BAND; j++) {
                leds[offset + j] = ColorFromPalette(currentPalette, colorBufferA[offset + j], brightnessBuffer[offset + j]);
            }
        } else {
            for (int j = 0, k = LED_MATRIX_N_PER_BAND - 1; j < LED_MATRIX_N_PER_BAND; j++, k--) {
                leds[offset + k] = ColorFromPalette(currentPalette, colorBufferA[offset + j], brightnessBuffer[offset + j]);
            }
        }
    }
}

// clang-format off
static float gaussianKernel[5][5]{
    {1,  4,  7,  4, 1},
    {4, 16, 26, 16, 4},
    {7, 26, 41, 26, 7},
    {4, 16, 26, 16, 4},
    {1,  4,  7,  4, 1}
};
// clang-format on

static void gaussianBlur(int nCols, int nRows, uint8_t *inp, uint8_t *out) {
    const int margin = 2;

    float sum = 0.0;
    int count = 0;

    for (int col = 0; col < nCols; col++) {
        for (int row = 0; row < nRows; row++) {
            sum = 0.0;
            count = 0;

            for (int x = col - margin; x <= col + margin; x++) {
                if (x == nCols) break;
                if (x < 0) x = 0;

                for (int y = row - margin; y <= row + margin; y++) {
                    if (y == nRows) break;
                    if (y < 0) y = 0;

                    float kernelValue = gaussianKernel[y - row + margin][x - col + margin];

                    count += kernelValue;
                    sum += kernelValue * inp[x * nRows + y];
                }
            }

            out[col * nRows + row] = uint8_t(round(sum / count));
        }
    }
}

static void updateColorBars(float *bands) {
    const float decay = 0.02;
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float d = bands[i] - bandsBuffer[i];
        if (d > 0.6) {
            bandsBuffer[i] = (bandsBuffer[i] * 1.0 + bands[i]) / 2.0;
        } else if (d > 0.2) {
            bandsBuffer[i] = (bandsBuffer[i] * 2.0 + bands[i]) / 3.0;
        } else if (d > 0.0) {
            bandsBuffer[i] = (bandsBuffer[i] * 3.0 + bands[i]) / 4.0;
        } else {
            bandsBuffer[i] = bandsBuffer[i] < decay ? 0.0 : bandsBuffer[i] - decay;
        }
    }

    for (int j = 0; j < LED_MATRIX_N; j++) {
        colorBufferA[j] = 1;
        brightnessBuffer[j] = 255;
    }

    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float band = bandsBuffer[i];
        if (band > 1.0) band = 1.0;

        int left = int(band * LED_MATRIX_N_PER_BAND * 255);
        for (int j = 0; i < LED_MATRIX_N_PER_BAND; j++) {

            colorBufferA[i * LED_MATRIX_N_PER_BAND + j] = 80 + j * 6;
            brightnessBuffer[i * LED_MATRIX_N_PER_BAND + j] = left > 255 ? 255 : left;

            left -= 255;
            if (left < 0) break;
        }
    }
}

static void updateSpectrum(float *bands) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float d = bands[i] - bandsBuffer[i];
        if (d > 0.6) {
            bandsBuffer[i] = (bandsBuffer[i] * 1.0 + bands[i]) / 2.0;
        } else if (d > 0.2) {
            bandsBuffer[i] = (bandsBuffer[i] * 2.0 + bands[i]) / 3.0;
        } else if (d > 0.0) {
            bandsBuffer[i] = (bandsBuffer[i] * 3.0 + bands[i]) / 4.0;
        } else {
            bandsBuffer[i] *= 0.92;
        }
    }

    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float band = bandsBuffer[i];
        if (band > 1.0) band = 1.0;
        uint8_t colorIndex = int(band * 255.0);

        for (int j = LED_MATRIX_N_PER_BAND - 1; j > 0; j--) {
            colorBufferA[i * LED_MATRIX_N_PER_BAND + j] = colorBufferA[i * LED_MATRIX_N_PER_BAND + j - 1];
        }
        colorBufferA[i * LED_MATRIX_N_PER_BAND] = colorIndex;
    }
}

static void updateFire(float *bands) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float d = bands[i] - bandsBuffer[i];
        if (d > 0.0) {
            bandsBuffer[i] = bands[i];
        } else {
            bandsBuffer[i] *= 0.8;
        }
    }

    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float band = bandsBuffer[i];
        if (band > 1.0) band = 1.0;

        for (int j = LED_MATRIX_N_PER_BAND - 1; j > 0; j--) {
            colorBufferB[i * LED_MATRIX_N_PER_BAND + j] = colorBufferB[i * LED_MATRIX_N_PER_BAND + j - 1] * 0.975;
        }
        colorBufferB[i * LED_MATRIX_N_PER_BAND] += int(band * 255.0);
        colorBufferB[i * LED_MATRIX_N_PER_BAND] /= 2.0;
    }

    gaussianBlur(LED_MATRIX_N_BANDS, LED_MATRIX_N_PER_BAND, colorBufferB, colorBufferA);
}

void updateVisualization(float *bands) {
    switch (currentVisualization) {
        case VISUALIZATION_TYPE_BARS:
            updateColorBars(bands);
            break;
        case VISUALIZATION_TYPE_SPECTRUM:
            updateSpectrum(bands);
            break;
        case VISUALIZATION_TYPE_FIRE:
            updateFire(bands);
            break;
    }
    pushBuffer();
}

void showVisualization() {
    FastLED.show();
}
