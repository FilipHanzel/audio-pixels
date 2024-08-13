#include <Arduino.h>

#define FASTLED_INTERNAL  // silence FastLED SPI warning

#include <FastLED.h>

#define DEBUG

#include "audio.h"
#include "buttons.h"
#include "config.h"
#include "macros.h"

#define DEFAULT_AUDIO_SOURCE AUDIO_SOURCE_LINE_IN

TaskHandle_t controlerTaskHandle;
TaskHandle_t executorTaskHandle;
void controlerTask(void *pvParameters);
void executorTask(void *pvParameters);

typedef enum {
    set_audio_source,
} CommandType;

typedef struct {
    CommandType type;
    union {
        AudioSource audioSource;
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

void controlerTask(void *pvParameters) {
    ButtonDebounceState audioSourceBtnState;
    AudioSource audioSource = DEFAULT_AUDIO_SOURCE;

    pinMode(AUDIO_SOURCE_BUTTON_PIN, INPUT_PULLUP);

    while (true) {
        if (debouncedRelease(&audioSourceBtnState, digitalRead(AUDIO_SOURCE_BUTTON_PIN))) {
            audioSource = audioSource == AUDIO_SOURCE_MIC ? AUDIO_SOURCE_LINE_IN : AUDIO_SOURCE_MIC;
            Command command = {
                .type = set_audio_source,
                .data = {.audioSource = audioSource},
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
        bandScale = max > bandScale ? max : bandScale * 0.999;

        for (int i = 0; i < AUDIO_N_BANDS; i++) {
            audioBands[i] /= bandScale * 0.8;
            audioBands[i] = audioBands[i] > 1.0 ? 1.0 : audioBands[i];

            audioBands[i] = audioBandsOld[i] * 0.4 + audioBands[i] * 0.6;
            audioBandsOld[i] = audioBands[i];

            ledBars[i] = audioBands[i] > ledBars[i] ? audioBands[i] : ledBars[i] * 0.94;
        }

        // LEDs

        for (int i = 0; i < LED_MATRIX_N_BANDS; i++) {
            int offset = 0;
            if (i > 0) offset -= 4;
            int n = LED_MATRIX_N_PER_BAND;
            if (i == 0) n -= 4;

            int toLight = int(ledBars[i] * n);
            if (toLight > n) {
                toLight = n;
            }
            int toSkip = n - toLight;

            if (i % 2 == 0) {
                for (int j = 0; j < toLight; j++) {
                    leds[offset + i * n + j] = CRGB::Red;
                }
                for (int j = 0; j < toSkip; j++) {
                    leds[offset + i * n + j + toLight] = CRGB::Black;
                }
            } else {
                for (int j = 0; j < toSkip; j++) {
                    leds[offset + i * LED_MATRIX_N_PER_BAND + j] = CRGB::Black;
                }
                for (int j = 0; j < toLight; j++) {
                    leds[offset + i * LED_MATRIX_N_PER_BAND + j + toSkip] = CRGB::Red;
                }
            }
        }
        FastLED.show();
    }
}
