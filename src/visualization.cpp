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

// clang-format off

DEFINE_GRADIENT_PALETTE(blank_gp){
      0, 0, 0, 0,
    255, 0, 0, 0
};

DEFINE_GRADIENT_PALETTE(red_gp){
      0,   0,   0,   0,
    128, 255,   0,   0,
    255, 255, 200, 200
};

DEFINE_GRADIENT_PALETTE(green_gp){
      0,   0,   0,   0,
    128,   0, 255,   0,
    255, 200, 255, 200
};

DEFINE_GRADIENT_PALETTE(blue_gp){
      0,   0,   0,   0,
    128,   0,   0, 255,
    255, 200, 200, 255
};

DEFINE_GRADIENT_PALETTE(heatmapGreen_gp){
      0,   0,   1,  0,
     64,   0, 255,  0,
    128, 255,   0,  0,
    192, 255,  85,  0,
    255, 255, 255, 50
};

DEFINE_GRADIENT_PALETTE(heatmapBlue_gp){
      0,   0,   0,   1,
     64,   0,   0, 255,
    128, 255,   0,   0,
    192, 255,  85,   0,
    255, 255, 255,  50
};

DEFINE_GRADIENT_PALETTE(magmaPink_gp){
      0,   0,   0,   4,
     30,  28,  16,  68,
     60,  79,  18, 123,
     90, 129,  37, 129,
    120, 181,  54, 103,
    150, 229,  80,  63,
    180, 252, 136,  37,
    210, 251, 194,  71,
    240, 254, 253, 191,
    255, 255, 255, 255
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
      0,   0,   3,   0,
     40,   0,   6,   0,
     55,   0,  40,   0,
     60,  45,  60,   0,
     70,  55,  70,   0,
     90, 220, 140,  10,
    255, 200, 210, 200
};

// clang-format on

const static CRGBPalette16 blankPalette = blank_gp;
const static CRGBPalette16 redPalette = red_gp;
const static CRGBPalette16 greenPalette = green_gp;
const static CRGBPalette16 bluePalette = blue_gp;
const static CRGBPalette16 heatmapGreenPalette = heatmapGreen_gp;
const static CRGBPalette16 heatmapBluePalette = heatmapBlue_gp;
const static CRGBPalette16 magmaPinkPalette = magmaPink_gp;
const static CRGBPalette16 fireRedPalette = fireRed_gp;
const static CRGBPalette16 fireBluePalette = fireBlue_gp;
const static CRGBPalette16 fireGreenPalette = fireGreen_gp;

static uint8_t bufferA[LED_MATRIX_N] = {0};
static uint8_t bufferB[LED_MATRIX_N] = {0};
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

void teardownVisualization(VisualizationType visualization) {
    if (currentVisualization == VISUALIZATION_TYPE_NONE) {
        PRINTF("Visualization is not set up. Halt!\n");
        while (true) continue;
    }
    currentVisualization = VISUALIZATION_TYPE_NONE;
    currentPalette = blankPalette;
    for (int i = 0; i < LED_MATRIX_N; i++) {
        bufferA[i] = 0;
        bufferB[i] = 0;
        leds[i] = CRGB::Black;
    }
}

static void pushBuffer() {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        int offset = i * LED_MATRIX_N_PER_BAND;

        if (i % 2 == 0) {
            for (int j = 0; j < LED_MATRIX_N_PER_BAND; j++) {
                leds[offset + j] = ColorFromPalette(currentPalette, bufferA[offset + j]);
            }
        } else {
            for (int j = 0, k = LED_MATRIX_N_PER_BAND - 1; j < LED_MATRIX_N_PER_BAND; j++, k--) {
                leds[offset + k] = ColorFromPalette(currentPalette, bufferA[offset + j]);
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

void gaussianBlur(int nCols, int nRows, uint8_t *inp, uint8_t *out) {
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

static void updateColorBars(float *bars) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float bar = bars[i];
        if (bar > 1.0) bar = 1.0;
        int toLight = int(bar * LED_MATRIX_N_PER_BAND);
        int toSkip = LED_MATRIX_N_PER_BAND - toLight;

        for (int j = 0; j < toLight; j++) {
            bufferA[i * LED_MATRIX_N_PER_BAND + j] = 128 + j * 3;
        }
        for (int j = 0; j < toSkip; j++) {
            bufferA[i * LED_MATRIX_N_PER_BAND + j + toLight] = 1;
        }
    }
}

static void updateSpectrum(float *bars) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float bar = bars[i];
        if (bar > 1.0) bar = 1.0;
        uint8_t colorIndex = int(bar * 255.0);

        for (int j = LED_MATRIX_N_PER_BAND - 1; j > 0; j--) {
            bufferA[i * LED_MATRIX_N_PER_BAND + j] = bufferA[i * LED_MATRIX_N_PER_BAND + j - 1];
        }
        bufferA[i * LED_MATRIX_N_PER_BAND] = colorIndex;
    }
}

static void updateFire(float *bars) {
    for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
        float bar = bars[i];
        if (bar > 1.0) bar = 1.0;

        for (int j = LED_MATRIX_N_PER_BAND - 1; j > 0; j--) {
            bufferB[i * LED_MATRIX_N_PER_BAND + j] = bufferB[i * LED_MATRIX_N_PER_BAND + j - 1] * 0.975;
        }
        bufferB[i * LED_MATRIX_N_PER_BAND] += int(bar * 255.0);
        bufferB[i * LED_MATRIX_N_PER_BAND] /= 2.0;
    }

    gaussianBlur(LED_MATRIX_N_BANDS, LED_MATRIX_N_PER_BAND, bufferB, bufferA);
}

void updateVisualization(float *bars) {
    switch (currentVisualization) {
        case VISUALIZATION_TYPE_BARS:
            updateColorBars(bars);
            break;
        case VISUALIZATION_TYPE_SPECTRUM:
            updateSpectrum(bars);
            break;
        case VISUALIZATION_TYPE_FIRE:
            updateFire(bars);
            break;
    }
    pushBuffer();
    FastLED.show();
}
