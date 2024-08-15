#include <Arduino.h>

#define FASTLED_INTERNAL  // silence FastLED SPI warning
#include <FastLED.h>

#include "audio.h"

//
// Calibration mode and iniput selection
//
#define AUDIO_SOURCE AUDIO_SOURCE_MIC
// #define AUDIO_SOURCE AUDIO_SOURCE_LINE_IN

// #define NOISE_CALIBRATION_MODE
#define BAND_CALIBRATION_MODE
//
//

#define N 512  // calibration length
#define NOISE_MARGIN 0.5

#if defined BAND_CALIBRATION_MODE && defined NOISE_CALIBRATION_MODE
#error "One mode at a time!"
#endif
#if !defined BAND_CALIBRATION_MODE && !defined NOISE_CALIBRATION_MODE
#error "At least one mode!"
#endif

// TODO: Get rid of LED matrix settings after moving LED controls to separate module
#define LED_MATRIX_DATA_PIN 5
#define LED_MATRIX_N_BANDS 16
#define LED_MATRIX_N_PER_BAND 23
// I soldered wrong way last 4 LEDs of first band and they died, hence the -4
#define LED_MATRIX_N (LED_MATRIX_N_BANDS * LED_MATRIX_N_PER_BAND - 4)

__attribute__((aligned(16))) float audioBands[AUDIO_N_BANDS] = {0.0};
__attribute__((aligned(16))) float table[AUDIO_N_BANDS] = {0.0};
int counter = 0;

CRGB leds[LED_MATRIX_N] = {CRGB::Black};
int animationCursor = 0;

void setup() {
    Serial.begin(115200);
    delayMicroseconds(500);

    setupAudioSource(AUDIO_SOURCE);
    setupAudioProcessing();

#ifndef NOISE_CALIBRATION_MODE
    setupAudioNoiseTable(AUDIO_SOURCE);
#endif

    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN, GRB>(leds, LED_MATRIX_N);
    FastLED.show();
}

void loop() {
    readAudioDataToBuffer();
    processAudioData(audioBands);

#ifdef NOISE_CALIBRATION_MODE

    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        if (table[i] < audioBands[i]) {
            table[i] = audioBands[i];
        }
    }

    if (counter++ >= N) {
        counter = 0;

        for (int i = 0; i < AUDIO_N_BANDS; i++) {
            table[i] *= (1.0 + NOISE_MARGIN);
        }

        Serial.printf("Noise table: {");
        for (int i = 0; i < AUDIO_N_BANDS - 1; i++) {
            Serial.printf("%.2f, ", table[i]);
        }
        Serial.printf("%.2f}\n", table[AUDIO_N_BANDS - 1]);

        memset(table, 0, sizeof(table));
    }

#endif

#ifdef BAND_CALIBRATION_MODE

    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        table[i] += audioBands[i];
    }
    if (++counter > N) {
        counter = 0;

        float max = 0.0;
        for (int i = 0; i < AUDIO_N_BANDS; i++) {
            if (table[i] == 0.0) {
                table[i] = 0.00001;
            }

            if (max < table[i]) {
                max = table[i];
            }
        }

        for (int i = 0; i < AUDIO_N_BANDS; i++) {
            table[i] = max / table[i];
        }

        Serial.printf("Calibration table: {");
        for (int i = 0; i < AUDIO_N_BANDS - 1; i++) {
            Serial.printf("%.2f, ", table[i]);
        }
        Serial.printf("%.2f}\n", table[AUDIO_N_BANDS - 1]);

        memset(table, 0, sizeof(table));
    }

#endif

    // I found that wiith my current setup, driving the LEDs
    // causes noise that is picked up by the ADC.
    // I hope to get rid of that noise with RS485 transmission
    // and if that doesn't work, then external ADC.
    // In the meantime I added animation to the calibration
    // process, too make sure the noise is included.

    for (int i = 0; i < LED_MATRIX_N; i++) {
        leds[i] = CRGB::Black;
    }

    const int nColours = 3;
    CRGB colours[nColours] = {
        CRGB::Red,
        CRGB::Green,
        CRGB::Blue,
    };
    for (int j = 0; j < nColours; j++) {
        int band = (animationCursor + j) % LED_MATRIX_N_BANDS;
        int offset = band > 0 ? -4 : 0;  // offset is there to take dead LEDs into account
        int n = band > 0 ? LED_MATRIX_N_PER_BAND : LED_MATRIX_N_PER_BAND - 4;

        for (int k = 0; k < n; k++) {
            leds[band * LED_MATRIX_N_PER_BAND + offset + k] = colours[j];
        }
    }
    animationCursor = ++animationCursor % LED_MATRIX_N_BANDS;

    FastLED.show();
}
