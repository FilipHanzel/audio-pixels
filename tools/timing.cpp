#include <Arduino.h>

#define FASTLED_INTERNAL // Silence FastLED SPI warning
#include <FastLED.h>

#include "audio.h"
#include "visualization.h"

// Config
#define AUDIO_SOURCE       AUDIO_SOURCE_LINE_IN
#define VISUALIZATION_TYPE VISUALIZATION_TYPE_FIRE

float timeStart = 0.0;
uint loops = 0;

#define N_LOOPS              512
#define TIME_MEASURE_START   timeStart = micros()
#define TIME_MEASURE_END(dt) dt += micros() - timeStart

float dt_readAudioDataToBuffer = 0.0;
float dt_processAudioData = 0.0;
float dt_scaleAudioData = 0.0;
float dt_updateVisualization = 0.0;
float dt_showVisualization = 0.0;

void checkTimings() {
    if (loops++ >= N_LOOPS) {
        loops = 0;

        Serial.printf("Timings:\n");
        Serial.printf("  dt_readAudioDataToBuffer: %.2fus per iteration\n", dt_readAudioDataToBuffer / N_LOOPS);
        Serial.printf("  dt_processAudioData:      %.2fus per iteration\n", dt_processAudioData / N_LOOPS);
        Serial.printf("  dt_scaleAudioData:        %.2fus per iteration\n", dt_scaleAudioData / N_LOOPS);
        Serial.printf("  dt_updateVisualization:   %.2fus per iteration\n", dt_updateVisualization / N_LOOPS);
        Serial.printf("  dt_showVisualization:     %.2fus per iteration\n", dt_showVisualization / N_LOOPS);

        float dt_totalAverage = 0.0;
        dt_totalAverage += dt_readAudioDataToBuffer;
        dt_totalAverage += dt_processAudioData;
        dt_totalAverage += dt_scaleAudioData;
        dt_totalAverage += dt_updateVisualization;
        dt_totalAverage += dt_showVisualization;
        Serial.printf("  dt_totalAverage:          %.2fus per iteration\n", dt_totalAverage / N_LOOPS);

        dt_readAudioDataToBuffer = 0.0;
        dt_processAudioData = 0.0;
        dt_scaleAudioData = 0.0;
        dt_updateVisualization = 0.0;
        dt_showVisualization = 0.0;
    }
}

__attribute__((aligned(16))) float audioBands[AUDIO_N_BANDS] = {0.0};
__attribute__((aligned(16))) float audioBandsOld[AUDIO_N_BANDS] = {0.0};
float bandScale = 0;

void setup() {
    Serial.begin(115200);
    delayMicroseconds(500);

    setupAudioSource(AUDIO_SOURCE);
    setupAudioTables(AUDIO_SOURCE);
    setupAudioProcessing();

    setupLedStrip();
    setupVisualization(VISUALIZATION_TYPE);
    setVisualizationPalette(0);
}

void loop() {
    TIME_MEASURE_START;
    readAudioDataToBuffer();
    TIME_MEASURE_END(dt_readAudioDataToBuffer);

    TIME_MEASURE_START;
    processAudioData(audioBands);
    TIME_MEASURE_END(dt_processAudioData);

    TIME_MEASURE_START;
    scaleAudioData(audioBands);
    TIME_MEASURE_END(dt_scaleAudioData);

    TIME_MEASURE_START;
    updateVisualization(audioBands);
    TIME_MEASURE_END(dt_updateVisualization);

    TIME_MEASURE_START;
    showVisualization();
    TIME_MEASURE_END(dt_showVisualization);

    checkTimings();
}
