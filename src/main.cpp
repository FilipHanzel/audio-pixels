#include <Arduino.h>
#include <driver/i2s.h>

#define DEBUG true

#if DEBUG
#define PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

// Setup tasks and shared resources

TaskHandle_t controlerTaskHandle;
TaskHandle_t executorTaskHandle;
void controlerTask(void *pvParameters);
void executorTask(void *pvParameters);

int counterA = 0;
int counterB = 0;
int counterC = 0;

SemaphoreHandle_t xMutexCounterA = NULL;
SemaphoreHandle_t xMutexCounterB = NULL;
SemaphoreHandle_t xMutexCounterC = NULL;

void setup() {
    delayMicroseconds(500);

    xMutexCounterA = xSemaphoreCreateMutex();
    xMutexCounterB = xSemaphoreCreateMutex();
    xMutexCounterC = xSemaphoreCreateMutex();

#if DEBUG
    Serial.begin(115200);
#endif

    xTaskCreatePinnedToCore(controlerTask, "controlerTask", 8192, NULL, tskIDLE_PRIORITY, &controlerTaskHandle, 0);
    xTaskCreatePinnedToCore(executorTask, "executorTask", 8192, NULL, tskIDLE_PRIORITY, &executorTaskHandle, 1);
}

// Get rid of the Arduino main loop
void loop() { vTaskDelete(NULL); }

// Control

/**
 * @brief Button state struct used in debouncing routine.
 */
typedef struct {
    byte stableState = HIGH;
    byte lastState = HIGH;
    unsigned long lastDebounceTime = 0;
} ButtonDebounceState;

#define BUTTON_DEBOUNCE_DELAY 20
#define TIME_PASSED_SINCE(T) (millis() - T)

/**
 * @brief Button debouncing routine.
 *
 * @param bds Pointer to the `ButtonDebounceState` struct for the button.
 * @param reading Unprocessed reading of the button state.
 *
 * @return `true` if button was released, `false` otherwise.
 */
bool debouncedRelease(ButtonDebounceState *bds, int reading) {
    bool is_release = false;

    if (reading != bds->lastState) {
        bds->lastDebounceTime = millis();
    }

    if (TIME_PASSED_SINCE(bds->lastDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
        if (reading != bds->stableState) {
            bds->stableState = reading;

            if (bds->stableState == HIGH) {
                is_release = true;
            }
        }
    }
    bds->lastState = reading;

    return is_release;
}

#define BUTTON_A_PIN 25
#define BUTTON_B_PIN 26
#define BUTTON_C_PIN 27

void controlerTask(void *pvParameters) {
    ButtonDebounceState buttonDebounceStateA;
    ButtonDebounceState buttonDebounceStateB;
    ButtonDebounceState buttonDebounceStateC;

    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);
    pinMode(BUTTON_C_PIN, INPUT_PULLUP);

    while (true) {
        if (debouncedRelease(&buttonDebounceStateA, digitalRead(BUTTON_A_PIN))) {
            if (xSemaphoreTake(xMutexCounterA, 200 / portTICK_PERIOD_MS) == pdTRUE) {
                ++counterA;
                PRINTF("counterA = %d\n", counterA);
                xSemaphoreGive(xMutexCounterA);
            }
        }

        if (debouncedRelease(&buttonDebounceStateB, digitalRead(BUTTON_B_PIN))) {
            if (xSemaphoreTake(xMutexCounterB, 200 / portTICK_PERIOD_MS) == pdTRUE) {
                ++counterB;
                PRINTF("counterB = %d\n", counterB);
                xSemaphoreGive(xMutexCounterB);
            }
        }

        if (debouncedRelease(&buttonDebounceStateC, digitalRead(BUTTON_C_PIN))) {
            if (xSemaphoreTake(xMutexCounterC, 200 / portTICK_PERIOD_MS) == pdTRUE) {
                ++counterC;
                PRINTF("counterC = %d\n", counterC);
                xSemaphoreGive(xMutexCounterC);
            }
        }
    }
}

// Audio processing + LEDs controll
// TODO: Use WM8960 instead of on-board ADC.

#define N_SAMPLES 1024
#define SAMPLING_RATE 44100

#define INPUT_ATTENUATION ADC_ATTEN_DB_11
#define INPUT_MIC ADC1_CHANNEL_4   // GPIO32
#define INPUT_LINE ADC1_CHANNEL_5  // GPIO33

#define DEFAULT_INPUT INPUT_LINE

#define I2S_PORT I2S_NUM_0

void setupAudioInput();
void setAudioInputPin(adc1_channel_t channel);

/**
 *
 */
void setupAudioInput() {
    const i2s_driver_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = SAMPLING_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = N_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        PRINTF("Error installing I2S: %d. Halt!\n", err);
        while (true);
    }

    setAudioInputPin(DEFAULT_INPUT);
}

/**
 *
 */
void setAudioInputPin(adc1_channel_t channel) {
    esp_err_t err;

    err = i2s_adc_disable(I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error disabling ADC: %d. Halt!\n", err);
        while (true);
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, channel);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC: %d. Halt!\n", err);
        while (true);
    }

    err = adc1_config_channel_atten(channel, INPUT_ATTENUATION);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC attenuation: %d. Halt!\n", err);
        while (true);
    }

    err = i2s_adc_enable(I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error enabling ADC: %d. Halt!\n", err);
        while (true);
    }
}

void executorTask(void *pvParameters) {
    setupAudioInput();

    // TODO: Use buttonA to switch between mic and line-in
    setAudioInputPin(INPUT_MIC);

    int16_t audio_buffer[N_SAMPLES] = {0};

    int16_t max = 0.0;
    int16_t min = 4096.0;

#define ADC_MASK 0x0FFF

    while (true) {
        size_t bytes_read = 0;
        i2s_read(I2S_PORT, audio_buffer, N_SAMPLES, &bytes_read, portMAX_DELAY);

        for (int i = 0; i < N_SAMPLES; i++) {
            audio_buffer[i] &= ADC_MASK;
            if (max < audio_buffer[i]) max = audio_buffer[i];
            if (audio_buffer[i] > 0.0) {
                if (min > audio_buffer[i]) min = audio_buffer[i];
            }
        }
        PRINTF("%d %d\n", min, max);
        max = 0.0;
        min = 4096.0;
    }
}

