#include <Arduino.h>

// Defining to silence FastLED SPI warning
#define FASTLED_INTERNAL

#include <FastLED.h>
#include <driver/i2s.h>
#include <esp_dsp.h>

#include "buttons.h"

#define DEBUG

#ifdef DEBUG
#define PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/**
 * Tasks setup.
 */

TaskHandle_t controlerTaskHandle;
TaskHandle_t executorTaskHandle;
void controlerTask(void *pvParameters);
void executorTask(void *pvParameters);

void setupTasks() {
    xTaskCreatePinnedToCore(controlerTask, "controlerTask", 8192, NULL, tskIDLE_PRIORITY, &controlerTaskHandle, 0);
    xTaskCreatePinnedToCore(executorTask, "executorTask", 8192, NULL, tskIDLE_PRIORITY, &executorTaskHandle, 1);
}

/**
 * Setup communication between tasks.
 *
 * Controller talks to executor with Command structs through commandQueue.
 * Executor does not talk to controller. There is no shared state.
 */

/**
 * @brief Types of commands sent from controller to executor.
 */
typedef enum {
    set_audio_source,
} CommandType;

/**
 * @brief Available audio sources.
 */
typedef enum {
    audio_source_mic,
    audio_source_line_in,
} AudioSource;
AudioSource audioSource = audio_source_line_in;

/**
 * @brief Commands sent from controller to executor.
 */
typedef struct {
    CommandType type;
    union {
        AudioSource audioSource;
    } data;
} Command;

QueueHandle_t commandQueue = NULL;

void setupCommandQueue() {
    commandQueue = xQueueCreate(32, sizeof(Command));
    if (commandQueue == NULL) {
        PRINTF("Error creating command queue. Likely due to memory. Halt!");
        while (true);
    }
}

/**
 * Entry point.
 */
void setup() {
#ifdef DEBUG
    Serial.begin(115200);
#endif

    delayMicroseconds(500);

    setupCommandQueue();
    setupTasks();
}

// Get rid of the Arduino main loop
void loop() { vTaskDelete(NULL); }

/**
 *
 * Controler.
 *
 */

#define AUDIO_SOURCE_BUTTON_PIN 25
#define BUTTON_B_PIN 26
#define BUTTON_C_PIN 27

void controlerTask(void *pvParameters) {
    ButtonDebounceState sourceButtonDebounceState;
    ButtonDebounceState buttonDebounceStateB;
    ButtonDebounceState buttonDebounceStateC;

    pinMode(AUDIO_SOURCE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);
    pinMode(BUTTON_C_PIN, INPUT_PULLUP);

    Command commandToSend;
    while (true) {
        if (debouncedRelease(&sourceButtonDebounceState, digitalRead(AUDIO_SOURCE_BUTTON_PIN))) {
            switch (audioSource) {
                case audio_source_line_in:
                    audioSource = audio_source_mic;
                    break;
                case audio_source_mic:
                    audioSource = audio_source_line_in;
                    break;
            }
            commandToSend = {
                .type = set_audio_source,
                .data = {.audioSource = audioSource},
            };
            xQueueSendToBack(commandQueue, &commandToSend, pdMS_TO_TICKS(200));
        }

        if (debouncedRelease(&buttonDebounceStateB, digitalRead(BUTTON_B_PIN))) {
            PRINTF("Button B (pin %d) pressed.", BUTTON_B_PIN);
        }

        if (debouncedRelease(&buttonDebounceStateC, digitalRead(BUTTON_C_PIN))) {
            PRINTF("Button C (pin %d) pressed.", BUTTON_C_PIN);
        }
    }
}

/**
 *
 * Executor.
 *
 */

#define INPUT_ATTENUATION ADC_ATTEN_DB_11
#define INPUT_MIC_PIN ADC1_CHANNEL_4      // GPIO32
#define INPUT_LINE_IN_PIN ADC1_CHANNEL_5  // GPIO33

#define I2S_PORT I2S_NUM_0

#define N_SAMPLES 1024
#define SAMPLING_RATE 44100

void setupAudioInput();
void setAudioInputPin(adc1_channel_t channel);

void setupAudioInput() {
    const i2s_driver_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = SAMPLING_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = N_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        PRINTF("Error installing I2S: 0x(%x). Halt!\n", err);
        while (true);
    }

    switch (audioSource) {
        case audio_source_mic:
            setAudioInputPin(INPUT_MIC_PIN);    
            break;
        case audio_source_line_in:
            setAudioInputPin(INPUT_LINE_IN_PIN);
            break;
    }

}

void setAudioInputPin(adc1_channel_t channel) {
    esp_err_t err;

    err = i2s_adc_disable(I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error disabling ADC: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, channel);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = adc1_config_channel_atten(channel, INPUT_ATTENUATION);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC attenuation: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = i2s_adc_enable(I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error enabling ADC: 0x(%x). Halt!\n", err);
        while (true);
    }
}

#define N_LED_BANDS 16
#define N_LEDS_PER_BAND 23
// I soldered wrong way last 4 leds of first band and they died.
// I'm also using one led at the beginning of the strip, to act
// as a level shifter, running on lower voltage:
//  https://hackaday.com/2017/01/20/cheating-at-5v-ws2812-control-to-use-a-3-3v-data-line/
#define N_LEDS (N_LED_BANDS * N_LEDS_PER_BAND - 4 + 1)
#define LED_DATA_PIN 5

__attribute__((aligned(16))) int16_t audioBuffer[N_SAMPLES] = {0};
// I tried applying different window functions to the audio signal,
// but it caused the bands to react differently for the same signal
// played multiple times (can be easily tested with online metronome).
// I commented out the window for now.
// 
// TODO: Test different windows when FFT is done on two samples at once
//       (previous sample and current one).
// 
// TODO: Try flatter window.
// 
// __attribute__((aligned(16))) float window[N_SAMPLES];
__attribute__((aligned(16))) float fftBuffer[N_SAMPLES * 2];

__attribute__((aligned(16))) float barkThresholds[N_LED_BANDS];
__attribute__((aligned(16))) float bandsOld[N_LED_BANDS] = {0};
__attribute__((aligned(16))) float bands[N_LED_BANDS] = {0};

__attribute__((aligned(16))) float ledBars[N_LED_BANDS] = {0};

// Audio processing part has to be calibrated for noise and band values,
// to make max band values roughly equal.
// 
// TODO: Calibrate for mic and line-in separately.
// 
// TODO: Document calibration process.
// 
// TODO: Try brown noise for band calibration.
// 
// #define BAND_CALIBRATION_MODE
// #define NOISE_THRESHOLD_CALIBRATION_MODE

#ifdef BAND_CALIBRATION_MODE
int loopCounter = 0;
__attribute__((aligned(16))) float calibrationTable[N_LED_BANDS] = {0.0};
#else
__attribute__((aligned(16))) float calibrationTable[N_LED_BANDS] = {
    2.249558,
    3.024515,
    3.306966,
    3.521792,
    4.001316,
    3.380087,
    3.409574,
    3.200538,
    2.980788,
    2.646222,
    2.704367,
    2.598766,
    2.216094,
    1.761241,
    1.000000,
    1.168663,
};
float bandLimit = 144212.54;
#endif

#ifdef NOISE_THRESHOLD_CALIBRATION_MODE
int loopCounter = 0;
__attribute__((aligned(16))) float thresholdsTable[N_LED_BANDS] = {0.0};
#else
__attribute__((aligned(16))) float thresholdsTable[N_LED_BANDS] = {
    10752.555664,
    6771.755371,
    6128.288086,
    4707.418457,
    4600.379395,
    6389.363281,
    8815.477539,
    14798.073242,
    12872.862305,
    16887.699219,
    19827.494141,
    28784.611328,
    41485.410156,
    65206.089844,
    67109.109375,
    73847.992188,
};
#endif

void executorTask(void *pvParameters) {
    Command receivedCommand;

    setupAudioInput();
    size_t bytesRead = 0;

    CRGB leds[N_LEDS] = {CRGB::Black};
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, N_LEDS);

    // Indicator & level shifter LED
    leds[0] = CRGB(10, 30, 10);

    FastLED.show();

    // FFT setup

    esp_err_t ret;
    ret = dsps_fft2r_init_fc32(NULL, N_SAMPLES);
    if (ret != ESP_OK) {
        PRINTF("Not possible to initialize FFT2R. Error: 0x(%x). Halt!", ret);
        while (true);
    }

    // Window coefficients

    // dsps_wind_hann_f32(window, N_SAMPLES);
    // dsps_wind_nuttall_f32(window, N_SAMPLES);
    // dsps_wind_flat_top_f32(window, N_SAMPLES);
    // dsps_wind_blackman_f32(window, N_SAMPLES);
    // dsps_wind_blackman_harris_f32(window, N_SAMPLES);

    // Bark thresholds

    float barkStep = 6.0 * asinh(SAMPLING_RATE / 2.0 / 600.0) / N_LED_BANDS;
    for (int i = 0; i < N_LED_BANDS; i++) {
        barkThresholds[i] = 600.0 * sinh(barkStep * (i + 1) / 6.0);
    }

    // Dynamic band scaling
    float maxBandValue = 5000.0;


    while (true) {
        if (xQueueReceive(commandQueue, &receivedCommand, 0) == pdPASS) {
            switch (receivedCommand.type) {
                // TODO: Extract command handling to separate function
                case set_audio_source:
                    switch (receivedCommand.data.audioSource) {
                        case audio_source_mic:
                            setAudioInputPin(INPUT_MIC_PIN);
                            break;
                        case audio_source_line_in:
                            setAudioInputPin(INPUT_LINE_IN_PIN);
                            break;
                    }
                    break;
            }
        }
        
        i2s_read(I2S_PORT, audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);

        // Copy from buffer, shift by the average (and apply a window?)
        float avg = 0.0;
        for (int i = 0; i < N_SAMPLES; i++) {
            // TODO: Test scaling down audioBuffer value to [0.0, 1.0] range.
            fftBuffer[i * 2 + 0] = float(audioBuffer[i] & 0x0FFF);
            fftBuffer[i * 2 + 1] = 0;

            avg += fftBuffer[i * 2 + 0];
        }
        avg /= N_SAMPLES;
        for (int i = 0; i < N_SAMPLES; i++) {
            fftBuffer[i * 2 + 0] -= avg;
            // fftBuffer[i * 2 + 0] *= window[i];
        }

        // Actual FFT
        ret = dsps_fft2r_fc32(fftBuffer, N_SAMPLES);
        if (ret != ESP_OK) {
            PRINTF("FFT2R error: 0x(%x). Halt!", ret);
            while (true);
        }
        ret = dsps_bit_rev2r_fc32(fftBuffer, N_SAMPLES);
        if (ret != ESP_OK) {
            PRINTF("FFT2R bit reverse error: 0x(%x). Halt!", ret);
            while (true);
        }

        // Convert to power spectrum
        for (int i = 0; i < N_SAMPLES; i++) {
            fftBuffer[i] = sqrtf(fftBuffer[i * 2 + 0] * fftBuffer[i * 2 + 0] + fftBuffer[i * 2 + 1] * fftBuffer[i * 2 + 1]);
        }

        // Split to bands
        memset(bands, 0.0, sizeof(bands));
        int bandIdx = 0;
        for (int i = 1; i < N_SAMPLES / 2; i++) {
            float frequency = i * SAMPLING_RATE / N_SAMPLES;
            if (barkThresholds[bandIdx] < frequency) {
                bandIdx++;
                if (bandIdx > N_LED_BANDS) {
                    PRINTF("Frequency band grouping error. Halt!");
                    while (true);
                }
            }
            bands[bandIdx] += fftBuffer[i];
        }

        // Ouput for debugging with plotter
        
        // for (int i = 0; i < N_LED_BANDS - 1; i++) {
        //     PRINTF("%f ", bands[i]);
        // }
        // PRINTF("%f\n", bands[N_LED_BANDS - 1]);
        // continue;


#ifdef NOISE_THRESHOLD_CALIBRATION_MODE

        for (int i = 0; i < N_LED_BANDS; i++) {
            thresholdsTable[i] += bands[i];
        }

        if (++loopCounter > 2048) {
            loopCounter = 0;

            for (int i = 0; i < N_LED_BANDS; i++) {
                thresholdsTable[i] /= 2048.0;
            }

            PRINTF("Thresholding table: {");
            for (int i = 0; i < N_LED_BANDS - 1; i++) {
                PRINTF("%f, ", thresholdsTable[i]);
            }
            PRINTF("%f}\n", thresholdsTable[N_LED_BANDS - 1]);

            memset(thresholdsTable, 0.0, sizeof(thresholdsTable));
        }
#else

        // Apply noise tresholding

        for (int i = 0; i < N_LED_BANDS; i++) {
            bands[i] -= thresholdsTable[i];
            bands[i] = bands[i] < 0.0 ? 0.0 : bands[i];
        }

#if defined BAND_CALIBRATION_MODE

        for (int i = 0; i < N_LED_BANDS; i++) {
            calibrationTable[i] += bands[i];
        }
        if (++loopCounter > 2048) {
            loopCounter = 0;

            float max = 0.0;
            for (int i = 0; i < N_LED_BANDS; i++) {
                if (max < calibrationTable[i]) {
                    max = calibrationTable[i];
                }
            }

            for (int i = 0; i < N_LED_BANDS; i++) {
                calibrationTable[i] = max / calibrationTable[i];
            }

            PRINTF("Calibration table: {");
            for (int i = 0; i < N_LED_BANDS - 1; i++) {
                PRINTF("%f, ", calibrationTable[i]);
            }
            PRINTF("%f}\n", calibrationTable[N_LED_BANDS - 1]);

            memset(calibrationTable, 0.0, sizeof(calibrationTable));
        }

#else
        // Calibration and scale adjustment
        float max = 0.0;
        for (int i = 0; i < N_LED_BANDS; i++) {
            bands[i] *= calibrationTable[i];
            max = max < bands[i] ? bands[i] : max;
        }
        if (max > maxBandValue) {
            maxBandValue = max;
        } else {
            maxBandValue *= 0.999;
        }


        for (int i = 0; i < N_LED_BANDS; i++) {

            // Scaling
            bands[i] /= maxBandValue * 0.8;
            bands[i] = bands[i] > 1.0 ? 1.0 : bands[i];

            // Smoothing
            bands[i] = bandsOld[i] * 0.4 + bands[i] * 0.6;
            bandsOld[i] = bands[i];

            // Translating to LED values
            ledBars[i] *= 0.94;
            if (bands[i] > ledBars[i]) {
                ledBars[i] = bands[i];
            }
        }

        // Updating LEDs

        for (int i = 0; i < N_LED_BANDS; i++) {
            int offset = 1;
            if (i > 0) offset -= 4;
            int n = N_LEDS_PER_BAND;
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
                    leds[offset + i * N_LEDS_PER_BAND + j] = CRGB::Black;
                }
                for (int j = 0; j < toLight; j++) {
                    leds[offset + i * N_LEDS_PER_BAND + j + toSkip] = CRGB::Red;
                }
            }
        }
        FastLED.show();

#endif  // NOISE_THRESHOLD_CALIBRATION_MODE
#endif  // BAND_CALIBRATION_MODE

    }
}
