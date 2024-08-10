#include <Arduino.h>
#include "driver/i2s.h"

#define I2S_WS 18
#define I2S_SD 19
#define I2S_SCK 21

void setup() {
    Serial.begin(115200);

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
    };
    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
        Serial.print("Error during driver install");
        while (true);
    }
    delay(500);

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD,
    };
    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
        Serial.print("Error during pin setup");
        while (true);
    }
    delay(500);

    if (i2s_start(I2S_NUM_0) != ESP_OK) {
        Serial.print("Error during i2s start");
        while (true);
    }
    delay(500);
}

void loop() {
    const int num_samples = 128;
    int32_t samples[num_samples] = {0};

    size_t bytes_read = 0;
    i2s_read(I2S_NUM_0, samples, num_samples * sizeof(int32_t), &bytes_read, portMAX_DELAY);

    float mean = 0.0;
    float max = 0.0;
    float min = 8388607.0; // max for signed 24 bit

    for (int i = 0; i < num_samples; i++) {
        int32_t data = samples[i] >> 8;

        mean += data;

        if (data > max) max = data;
        if (data < min) min = data;
    }
    mean /= num_samples;
    Serial.printf("%f %f %f\n", min, mean, max);

    // while(true);
}
