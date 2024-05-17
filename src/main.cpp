#include <Arduino.h>

TaskHandle_t controlerTaskHandle;
TaskHandle_t executorTaskHandle;
void controlerTask(void *pvParameters);
void executorTask(void *pvParameters);

void setup() {
    delayMicroseconds(500);
    Serial.begin(115200);

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

#define BUTTON_DEBOUNCE_DELAY 20
#define TIME_PASSED_SINCE(T) (millis() - T)

void controlerTask(void *pvParameters) {
    struct Button {
        byte pin;
        byte buttonState = HIGH;
        byte lastButtonState = HIGH;
        unsigned long lastDebounceTime = 0;
    } buttonA, buttonB, buttonC;

    buttonA.pin = 25;
    buttonB.pin = 26;
    buttonC.pin = 27;

    pinMode(buttonA.pin, INPUT_PULLUP);
    pinMode(buttonB.pin, INPUT_PULLUP);
    pinMode(buttonC.pin, INPUT_PULLUP);

    xMutexCounterA = xSemaphoreCreateMutex();
    xMutexCounterB = xSemaphoreCreateMutex();
    xMutexCounterC = xSemaphoreCreateMutex();

    while (true) {
        int readingA = digitalRead(buttonA.pin);
        if (readingA != buttonA.lastButtonState) buttonA.lastDebounceTime = millis();
        if (TIME_PASSED_SINCE(buttonA.lastDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
            if (readingA != buttonA.buttonState) {
                buttonA.buttonState = readingA;

                if (buttonA.buttonState == HIGH) {
                    if (xSemaphoreTake(xMutexCounterA, 200 / portTICK_PERIOD_MS) == pdTRUE) {
                        Serial.print("counterA = ");
                        Serial.println(++counterA);
                        xSemaphoreGive(xMutexCounterA);
                    }
                }
            }
        }
        buttonA.lastButtonState = readingA;

        int readingB = digitalRead(buttonB.pin);
        if (readingB != buttonB.lastButtonState) buttonB.lastDebounceTime = millis();
        if (TIME_PASSED_SINCE(buttonB.lastDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
            if (readingB != buttonB.buttonState) {
                buttonB.buttonState = readingB;

                if (buttonB.buttonState == HIGH) {
                    if (xSemaphoreTake(xMutexCounterB, 200 / portTICK_PERIOD_MS) == pdTRUE) {
                        Serial.print("counterB = ");
                        Serial.println(++counterB);
                        xSemaphoreGive(xMutexCounterB);
                    }
                }
            }
        }
        buttonB.lastButtonState = readingB;

        int readingC = digitalRead(buttonC.pin);
        if (readingC != buttonC.lastButtonState) buttonC.lastDebounceTime = millis();
        if (TIME_PASSED_SINCE(buttonC.lastDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
            if (readingC != buttonC.buttonState) {
                buttonC.buttonState = readingC;

                if (buttonC.buttonState == HIGH) {
                    if (xSemaphoreTake(xMutexCounterC, 200 / portTICK_PERIOD_MS) == pdTRUE) {
                        Serial.print("counterC = ");
                        Serial.println(++counterC);
                        xSemaphoreGive(xMutexCounterC);
                    }
                }
            }
        }
        buttonC.lastButtonState = readingC;
    }
}

// Audio processing + LEDs

void executorTask(void *pvParameters) {
    while (true) {
    }
}
