/**
 * @file calibration.cpp
 * @brief Audio input calibration utility.
 *
 * An alternative to the main loop that allows for the calibration of the audio signal.
 * The calibration process ensures accurate audio processing by adjusting noise levels
 * and band values.
 *
 * @details
 * The calibration process involves two main steps:
 *
 * **Noise Calibration**
 * Set `NOISE_CALIBRATION_MODE` and upload the code to the ESP32. During this phase,
 * ensure no music is playing. For microphone calibration, perform this in a quiet room.
 * The noise calibration table will be periodically printed via the serial output.
 * Use the new noise calibration values to update the noise tables in `audio.cpp`.
 * Signals below the noise values will be ignored in future processing.
 *
 * **Band Calibration**
 * Set `BAND_CALIBRATION_MODE` and upload the code to the ESP32. Play pink noise loudly
 * during this phase. For microphone calibration, ensure there are no additional sounds
 * besides the pink noise. The band calibration values will be periodically printed
 * via the serial output. Use these values to update the calibration table in `audio.cpp`.
 * The band calibration step requires the noise table to be correctly calibrated beforehand.
 * If some values in the calibration table appear unusually large, this indicates an
 * error in the calibration process. In this case, try increasing the noise volume
 * and/or moving the speakers closer to the microphone.
 *
 * **Repeat the calibration as needed.** You can adjust the following parameters:
 * - `N`: The number of samples to take for the calibration.
 * - `NOISE_MARGIN`: How much extra noise value to include.
 *
 * @note
 * - Ensure the correct audio source is selected before starting the calibration.
 * - Ensure the correct calibration mode is selected before running the calibration.
 */

#include <Arduino.h>

#define FASTLED_INTERNAL // Silence FastLED SPI warning
#include <FastLED.h>

#include "audio.h"
#include "visualization.h"

// Input selection
#define AUDIO_SOURCE AUDIO_SOURCE_MIC

// Calibration mode
#define BAND_CALIBRATION_MODE

#define N            512 // Calibration length (number of samples)
#define NOISE_MARGIN 0.5 //

#if defined BAND_CALIBRATION_MODE && defined NOISE_CALIBRATION_MODE
#error "One mode at a time!"
#endif
#if !defined BAND_CALIBRATION_MODE && !defined NOISE_CALIBRATION_MODE
#error "At least one mode!"
#endif

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

        for (int k = 0; k < LED_MATRIX_N_PER_BAND; k++) {
            leds[band * LED_MATRIX_N_PER_BAND + k] = colours[j];
        }
    }
    animationCursor = ++animationCursor % LED_MATRIX_N_BANDS;

    FastLED.show();
}
