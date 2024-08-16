#include <Arduino.h>

#define FASTLED_INTERNAL  // silence FastLED SPI warning
#include <FastLED.h>

#define DEBUG

#include "audio.h"
#include "buttons.h"
#include "config.h"
#include "macros.h"
#include "visualization.h"

#define DEFAULT_AUDIO_SOURCE AUDIO_SOURCE_LINE_IN
#define DEFAULT_VISUALIZATION VISUALIZATION_RED_BARS

TaskHandle_t controlerTaskHandle;
TaskHandle_t executorTaskHandle;
void controlerTask(void *pvParameters);
void executorTask(void *pvParameters);

typedef enum {
    set_audio_source,
    set_visualization,
} CommandType;

typedef struct {
    CommandType type;
    union {
        AudioSource audioSource;
        Visualization visualization;
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
        while (true);
    }

    xTaskCreatePinnedToCore(executorTask, "executorTask", 8192, NULL, tskIDLE_PRIORITY, &executorTaskHandle, 1);
    xTaskCreatePinnedToCore(controlerTask, "controlerTask", 8192, NULL, tskIDLE_PRIORITY, &controlerTaskHandle, 0);
}

void loop() { vTaskDelete(NULL); }  // get rid of the Arduino main loop

#define AUDIO_SOURCE_BUTTON_PIN 25
#define VISUALIZATION_BUTTON_PIN 26

void controlerTask(void *pvParameters) {
    ButtonDebounceState audioSourceBtnState;
    ButtonDebounceState visualizationBtnState;
    AudioSource audioSource = DEFAULT_AUDIO_SOURCE;
    Visualization visualization = DEFAULT_VISUALIZATION;

    pinMode(AUDIO_SOURCE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(VISUALIZATION_BUTTON_PIN, INPUT_PULLUP);

    while (true) {
        if (debouncedRelease(&audioSourceBtnState, digitalRead(AUDIO_SOURCE_BUTTON_PIN))) {
            audioSource = audioSource == AUDIO_SOURCE_MIC ? AUDIO_SOURCE_LINE_IN : AUDIO_SOURCE_MIC;
            Command command = {
                .type = set_audio_source,
                .data = {.audioSource = audioSource},
            };
            xQueueSendToBack(commandQueue, &command, pdMS_TO_TICKS(200));
        }

        if (debouncedRelease(&visualizationBtnState, digitalRead(VISUALIZATION_BUTTON_PIN))) {
            visualization++;
            visualization %= VISUALIZATION_TYPE_MAX_VALUE + 1;
            Command command = {
                .type = set_visualization,
                .data = {.visualization = visualization},
            };
            xQueueSendToBack(commandQueue, &command, pdMS_TO_TICKS(200));
        }

        delay(5);
    }
}

#define LED_MATRIX_DATA_PIN 5
#define LED_MATRIX_N_BANDS 16
#define LED_MATRIX_N_PER_BAND 23
// I soldered wrong way last 4 leds of first band and they died, hence the -4 
#define LED_MATRIX_N (LED_MATRIX_N_BANDS * LED_MATRIX_N_PER_BAND - 4)

void executorTask(void *pvParameters) {
    AudioSource audioSource = DEFAULT_AUDIO_SOURCE;
    __attribute__((aligned(16))) float audioBands[AUDIO_N_BANDS] = {0.0};
    __attribute__((aligned(16))) float audioBandsOld[AUDIO_N_BANDS] = {0.0};
    setupAudioSource(audioSource);
    setupAudioTables(audioSource);
    setupAudioProcessing();
    float bandScale = 0.0;

    Visualization visualization = DEFAULT_VISUALIZATION;
    CRGB leds[LED_MATRIX_N] = {CRGB::Black};
    FastLED.addLeds<WS2812B, LED_MATRIX_DATA_PIN, GRB>(leds, LED_MATRIX_N);
    FastLED.show();
    __attribute__((aligned(16))) float ledBars[LED_MATRIX_N_BANDS] = {0.0};

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
                case set_visualization:
                    visualization = command.data.visualization;
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

        // LEDs

        CRGB color;
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


        for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
            int offset = i > 0 ? -4 : 0;
            int barLength = i > 0 ? LED_MATRIX_N_PER_BAND : LED_MATRIX_N_PER_BAND - 4;
            int toLight = int(ledBars[i] * LED_MATRIX_N_PER_BAND);
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
}
