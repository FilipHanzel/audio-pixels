#include <Arduino.h>
#include <driver/i2s.h>

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
#define DEFAULT_AUDIO_SOURCE audio_source_mic
/**
 * @brief Commands sent from controller to executor.
 */
typedef struct {
    CommandType type;
    // Based on command type, appropriate union member(s) should be set
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
 *
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
 * Controler.
 */

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

    AudioSource audioSource = DEFAULT_AUDIO_SOURCE;

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

// TODO: Use WM8960 instead of on-board ADC.

#define N_SAMPLES 1024
#define SAMPLING_RATE 44100

#define INPUT_ATTENUATION ADC_ATTEN_DB_11
#define INPUT_MIC_PIN ADC1_CHANNEL_4      // GPIO32
#define INPUT_LINE_IN_PIN ADC1_CHANNEL_5  // GPIO33

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

#if DEFAULT_AUDIO_SOURCE == audio_source_mic
    setAudioInputPin(INPUT_MIC_PIN);
#elif DEFAULT_AUDIO_SOURCE == audio_source_line_in
    setAudioInputPin(INPUT_LINE_IN_PIN);
#else
#error "Invalid DEFAULT_AUDIO_SOURCE."
#endif
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
    Command receivedCommand;

    setupAudioInput();

    int16_t audioBuffer[N_SAMPLES] = {0};

    int16_t max = 0.0;
    int16_t min = 4096.0;

    while (true) {
        if (xQueueReceive(commandQueue, &receivedCommand, 0) == pdPASS) {
            switch (receivedCommand.type) {
                // TODO: Extract command handling to separate function.
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

        size_t bytesRead = 0;
        i2s_read(I2S_PORT, audioBuffer, N_SAMPLES, &bytesRead, portMAX_DELAY);

        for (int i = 0; i < N_SAMPLES; i++) {
            // Mask unused bits
            audioBuffer[i] &= 0x0FFF;

            if (max < audioBuffer[i]) max = audioBuffer[i];
            if (audioBuffer[i] > 0.0) {
                if (min > audioBuffer[i]) min = audioBuffer[i];
            }
        }
        PRINTF("%d %d\n", min, max);
        max = 0.0;
        min = 4096.0;
    }
}

