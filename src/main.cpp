#include <Arduino.h>

#define DEBUG

#include "audio.h"
#include "buttons.h"
#include "config.h"
#include "macros.h"
#include "visualization.h"

#define DEFAULT_AUDIO_SOURCE       AUDIO_SOURCE_LINE_IN
#define DEFAULT_VISUALIZATION_TYPE VISUALIZATION_TYPE_BARS

TaskHandle_t controlerTaskHandle;
TaskHandle_t executorTaskHandle;
void controlerTask(void *pvParameters);
void executorTask(void *pvParameters);

typedef enum {
    set_audio_source,
    set_visualization_type,
    set_visualization_palette,
} CommandType;

typedef struct {
    CommandType type;
    union {
        AudioSource audioSource;
        VisualizationType visualizationType;
        VisualizationPalette visualizationPalette;
    } data;
} Command;
QueueHandle_t commandQueue = NULL;

void setup() {
#ifdef DEBUG
    Serial.begin(115200);
    delayMicroseconds(500);
#endif

    commandQueue = xQueueCreate(32, sizeof(Command));
    if (commandQueue == NULL) {
        PRINTF("Error creating command queue. Likely due to memory. Halt!\n");
        while (true) continue;
    }

    xTaskCreatePinnedToCore(executorTask, "executorTask", 8192, NULL, tskIDLE_PRIORITY, &executorTaskHandle, 1);
    xTaskCreatePinnedToCore(controlerTask, "controlerTask", 8192, NULL, tskIDLE_PRIORITY, &controlerTaskHandle, 0);
}

void loop() { vTaskDelete(NULL); } // get rid of the Arduino main loop

#define AUDIO_SOURCE_BUTTON_PIN          25
#define VISUALIZATION_TYPE_BUTTON_PIN    26
#define VISUALIZATION_PALETTE_BUTTON_PIN 27

void controlerTask(void *pvParameters) {
    ButtonDebounceState audioSourceBtnState;
    ButtonDebounceState visualizationTypeBtnState;
    ButtonDebounceState visualizationPaletteBtnState;

    AudioSource audioSource = DEFAULT_AUDIO_SOURCE;
    VisualizationType visualizationType = DEFAULT_VISUALIZATION_TYPE;
    VisualizationPalette visualizationPalette = 0;

    pinMode(AUDIO_SOURCE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(VISUALIZATION_TYPE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(VISUALIZATION_PALETTE_BUTTON_PIN, INPUT_PULLUP);

    while (true) {
        if (debouncedRelease(&audioSourceBtnState, digitalRead(AUDIO_SOURCE_BUTTON_PIN))) {
            audioSource++;
            audioSource %= AUDIO_SOURCE_TYPE_MAX_VALUE + 1;
            Command command = {
                .type = set_audio_source,
                .data = {.audioSource = audioSource},
            };
            xQueueSendToBack(commandQueue, &command, pdMS_TO_TICKS(200));
        }

        if (debouncedRelease(&visualizationTypeBtnState, digitalRead(VISUALIZATION_TYPE_BUTTON_PIN))) {
            visualizationType++;
            visualizationType %= VISUALIZATION_TYPE_MAX_VALUE + 1;
            Command command = {
                .type = set_visualization_type,
                .data = {.visualizationType = visualizationType},
            };
            xQueueSendToBack(commandQueue, &command, pdMS_TO_TICKS(200));
        }

        if (debouncedRelease(&visualizationPaletteBtnState, digitalRead(VISUALIZATION_PALETTE_BUTTON_PIN))) {
            int maxValue;
            switch (visualizationType) {
                case VISUALIZATION_TYPE_BARS:
                    maxValue = VISUALIZATION_PALETTE_BARS_MAX_VALUE;
                    break;
                case VISUALIZATION_TYPE_SPECTRUM:
                    maxValue = VISUALIZATION_PALETTE_SPECTRUM_MAX_VALUE;
                    break;
                case VISUALIZATION_TYPE_FIRE:
                    maxValue = VISUALIZATION_PALETTE_FIRE_MAX_VALUE;
                    break;
            }

            visualizationPalette++;
            visualizationPalette %= maxValue + 1;
            Command command = {
                .type = set_visualization_palette,
                .data = {.visualizationPalette = visualizationPalette},
            };
            xQueueSendToBack(commandQueue, &command, pdMS_TO_TICKS(200));
        }

        delay(5);
    }
}

void executorTask(void *pvParameters) {
    AudioSource audioSource = DEFAULT_AUDIO_SOURCE;
    __attribute__((aligned(16))) float audioBands[AUDIO_N_BANDS] = {0.0};
    __attribute__((aligned(16))) float audioBandsOld[AUDIO_N_BANDS] = {0.0};
    setupAudioSource(audioSource);
    setupAudioTables(audioSource);
    setupAudioProcessing();
    float bandScale = 0.0;

    VisualizationType visualizationType = DEFAULT_VISUALIZATION_TYPE;
    __attribute__((aligned(16))) float ledBars[LED_MATRIX_N_BANDS] = {0.0};
    setupLedStrip();
    setupVisualization(visualizationType);
    // TODO: Think if decouping `setupVisualization` and `setVisualizationPalette`
    //       is necessary. Handling both in one function might simplify things,
    //       assuming that color palette is the only state required by visualizations.
    setVisualizationPalette(0);

    Command command;
    while (true) {
        while (xQueueReceive(commandQueue, &command, 0) == pdPASS) {
            switch (command.type) {
                case set_audio_source:
                    teardownAudioSource(audioSource);
                    audioSource = command.data.audioSource;
                    setupAudioSource(audioSource);
                    setupAudioTables(audioSource);

                    bandScale = 0.0;
                    break;
                case set_visualization_type:
                    teardownVisualization(visualizationType);
                    visualizationType = command.data.visualizationType;
                    setupVisualization(visualizationType);
                    setVisualizationPalette(0);
                    break;
                case set_visualization_palette:
                    setVisualizationPalette(command.data.visualizationPalette);
                    break;
            }
        }

        readAudioDataToBuffer();

        // //
        // // min-max for testing
        // //
        // float _mean = 0.0;
        // float _max = 0.0;
        // float _min = 8388607.0;  // max for signed 24 bit
        // int32_t *internalAudioiBuffer;
        // getInternalAudioBuffer(&internalAudioiBuffer);
        // for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        //     float value = internalAudioiBuffer[i];
        //     _mean += value;
        //     if (value > _max) _max = value;
        //     if (value < _min) _min = value;
        // }
        // _mean /= AUDIO_N_SAMPLES;
        // PRINTF("%f %f %f\n", _min, _mean, _max);
        // continue;
        // //
        // //

        processAudioData(audioBands);

        float max = 0.0;
        for (int i = 0; i < AUDIO_N_BANDS; i++) {
            max = max < audioBands[i] ? audioBands[i] : max;
        }
        bandScale = max > bandScale ? max : bandScale * 0.99;
        bandScale = bandScale < 1.0 ? 1.0 : bandScale;

        for (int i = 0; i < AUDIO_N_BANDS; i++) {
            audioBands[i] /= bandScale * 0.8;
            audioBands[i] = audioBands[i] > 1.0 ? 1.0 : audioBands[i];

            audioBands[i] = audioBandsOld[i] * 0.4 + audioBands[i] * 0.6;
            audioBandsOld[i] = audioBands[i];

            ledBars[i] = audioBands[i] > ledBars[i] ? audioBands[i] : ledBars[i] * 0.94;
        }

        updateVisualization(ledBars);
    }
}
