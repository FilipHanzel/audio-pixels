#include <Arduino.h>

#define DEBUG true

#if DEBUG
    #define PRINT(...) Serial.print(__VA_ARGS__)
    #define PRINTLN(...) Serial.println(__VA_ARGS__)
#else
    #define PRINT(...)
    #define PRINTLN(...)
#endif

TaskHandle_t controlerTaskHandle;
TaskHandle_t executorTaskHandle;
void controlerTask(void *pvParameters);
void executorTask(void *pvParameters);

void setup() {
    delayMicroseconds(500);

    #if DEBUG
        Serial.begin(115200);
    #endif

    xTaskCreatePinnedToCore(controlerTask, "controlerTask", 8192, NULL, tskIDLE_PRIORITY, &controlerTaskHandle, 0);
    xTaskCreatePinnedToCore(executorTask, "executorTask", 8192, NULL, tskIDLE_PRIORITY, &executorTaskHandle, 1);
}

// Get rid of the Arduino main loop
void loop() { vTaskDelete(NULL); }

// Control

int counterA = 0;
int counterB = 0;
int counterC = 0;

SemaphoreHandle_t xMutexCounterA = NULL;
SemaphoreHandle_t xMutexCounterB = NULL;
SemaphoreHandle_t xMutexCounterC = NULL;

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

    xMutexCounterA = xSemaphoreCreateMutex();
    xMutexCounterB = xSemaphoreCreateMutex();
    xMutexCounterC = xSemaphoreCreateMutex();

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

// Audio processing + LEDs

void executorTask(void *pvParameters) {
    while (true) {
    }
}
