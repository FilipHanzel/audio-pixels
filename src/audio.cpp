#include "audio.h"

#include <driver/i2s.h>
#include <esp_dsp.h>
#include <math.h>
#include <string.h>

#include <cstddef>

#include "macros.h"

__attribute__((aligned(16))) static float noiseTableLineIn[AUDIO_N_BANDS] = {
    17204.0890624,
    10834.8085936,
    9805.2609376,
    7531.8695312,
    7360.607032,
    10222.981249,
    14104.764062,
    23676.917187,
    20596.579688,
    27020.318750,
    31723.990625,
    46055.378124,
    66376.656249,
    104329.743750,
    107374.575000,
    118156.787500,
};

__attribute__((aligned(16))) static float calibrationTableLineIn[AUDIO_N_BANDS] = {
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

__attribute__((aligned(16))) static float noiseTableMic[AUDIO_N_BANDS] = {
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
};

__attribute__((aligned(16))) static float calibrationTableMic[AUDIO_N_BANDS] = {
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
};

__attribute__((aligned(16))) static float noiseTableNone[AUDIO_N_BANDS] = {
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
};

__attribute__((aligned(16))) static float calibrationTableNone[AUDIO_N_BANDS] = {
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
    1.0,
};

static AudioSource currentAudioSource = AUDIO_SOURCE_NONE;
static float* currentNoiseTable = noiseTableNone;
static float* currentCalibrationTable = calibrationTableNone;

__attribute__((aligned(16))) static int32_t audioBuffer[AUDIO_N_SAMPLES] = {0};
__attribute__((aligned(16))) static float fftBuffer[AUDIO_N_SAMPLES * 2];
__attribute__((aligned(16))) static float barkThresholds[AUDIO_N_BANDS] = {0};

void setupAudioProcessing() {
    float barkStep = 6.0 * asinh(AUDIO_SAMPLING_RATE / 2.0 / 600.0) / AUDIO_N_BANDS;
    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        barkThresholds[i] = 600.0 * sinh(barkStep * (i + 1) / 6.0);
    }

    esp_err_t err = dsps_fft2r_init_fc32(NULL, AUDIO_N_SAMPLES);
    if (err != ESP_OK) {
        PRINTF("Not possible to initialize FFT2R. Error: 0x(%x). Halt!\n", err);
        while (true);
    }
}

static void setupMic() {
    const i2s_driver_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLING_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = AUDIO_N_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
    };

    const i2s_pin_config_t i2sPinConfig = {
        .bck_io_num = AUDIO_MIC_SCK_PIN,
        .ws_io_num = AUDIO_MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = AUDIO_MIC_SD_PIN,
    };

    esp_err_t err = i2s_driver_install(AUDIO_I2S_PORT, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        PRINTF("Error installing I2S driver: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = i2s_set_pin(AUDIO_I2S_PORT, &i2sPinConfig);
    if (err != ESP_OK) {
        PRINTF("Error setting I2S pin: 0x(%x). Halt!\n", err);
        while (true);
    }
}

static void setupLineIn() {
    const i2s_driver_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = AUDIO_SAMPLING_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = AUDIO_N_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
    };

    esp_err_t err = i2s_driver_install(AUDIO_I2S_PORT, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        PRINTF("Error installing I2S driver: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, AUDIO_LINE_IN_PIN);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC mode: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = adc1_config_channel_atten(AUDIO_LINE_IN_PIN, ADC_ATTEN_DB_12);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC attenuation: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = i2s_adc_enable(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error enabling ADC: 0x(%x). Halt!\n", err);
        while (true);
    }
}

void setupAudioSource(AudioSource audioSource) {
    if (currentAudioSource != AUDIO_SOURCE_NONE) {
        PRINTF("Audio source already set up. Halt!\n");
        while (true);
    }
    currentAudioSource = audioSource;

    if (audioSource == AUDIO_SOURCE_MIC) {
        setupMic();
    } else {
        setupLineIn();
    }
}

static void teardownMic() {
    esp_err_t err = i2s_driver_uninstall(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error uninstalling I2S driver: 0x(%x). Halt!\n", err);
        while (true);
    }
}

static void teardownLineIn() {
    esp_err_t err = i2s_adc_disable(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error disabling ADC: 0x(%x). Halt!\n", err);
        while (true);
    }

    err = i2s_driver_uninstall(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error uninstalling I2S driver: 0x(%x). Halt!\n", err);
        while (true);
    }

    // Without this, switching from line-in to mic won't work.
    // I found the function when I was looking for something
    // to reset the ADC and/or I2S module.
    err = adc_set_i2s_data_source(ADC_I2S_DATA_SRC_IO_SIG);
    if (err != ESP_OK) {
        PRINTF("Error uninstalling I2S driver: 0x(%x). Halt!\n", err);
        while (true);
    }
}

void teardownAudioSource(AudioSource audioSource) {
    if (currentAudioSource == AUDIO_SOURCE_NONE) {
        PRINTF("Audio source is not set up. Halt!\n");
        while (true);
    }
    currentAudioSource = AUDIO_SOURCE_NONE;

    if (audioSource == AUDIO_SOURCE_MIC) {
        teardownMic();
    } else {
        teardownLineIn();
    }
}

void readAudioDataToBuffer() {
    size_t bytesRead;
    i2s_read(AUDIO_I2S_PORT, audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);

    float avg = 0.0;
    uint8_t shift = currentAudioSource == AUDIO_SOURCE_MIC ? 12 : 16;
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        audioBuffer[i] >>= shift;
        avg += audioBuffer[i];
    }
    avg /= AUDIO_N_SAMPLES;
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        audioBuffer[i] -= avg;
    }
}

void setupAudioTables(AudioSource audioSource) {
    if (audioSource == AUDIO_SOURCE_MIC) {
        currentNoiseTable = noiseTableMic;
        currentCalibrationTable = calibrationTableMic;
    } else {
        currentNoiseTable = noiseTableLineIn;
        currentCalibrationTable = calibrationTableLineIn;
    }
}

void processAudioData(float* bands) {
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        fftBuffer[i * 2 + 0] = audioBuffer[i];
        fftBuffer[i * 2 + 1] = 0;
    }

    esp_err_t err = dsps_fft2r_fc32(fftBuffer, AUDIO_N_SAMPLES);
    if (err != ESP_OK) {
        PRINTF("FFT2R error: 0x(%x). Halt!\n", err);
        while (true);
    }
    err = dsps_bit_rev2r_fc32(fftBuffer, AUDIO_N_SAMPLES);
    if (err != ESP_OK) {
        PRINTF("FFT2R bit reverse error: 0x(%x). Halt!\n", err);
        while (true);
    }

    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        fftBuffer[i] = sqrtf(fftBuffer[i * 2 + 0] * fftBuffer[i * 2 + 0] + fftBuffer[i * 2 + 1] * fftBuffer[i * 2 + 1]);
    }

    memset(bands, 0.0, sizeof(float) * AUDIO_N_BANDS);
    int bandIdx = 0;
    for (int i = 1; i < AUDIO_N_SAMPLES / 2; i++) {
        float frequency = i * AUDIO_SAMPLING_RATE / AUDIO_N_SAMPLES;
        if (barkThresholds[bandIdx] < frequency) {
            bandIdx++;

            if (bandIdx > AUDIO_N_BANDS) {
                PRINTF("Frequency band grouping error. Halt!\n");
                while (true);
            }
        }
        bands[bandIdx] += fftBuffer[i];
    }

    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        bands[i] -= currentNoiseTable[i];
        bands[i] *= currentCalibrationTable[i];

        bands[i] = bands[i] < 0.0 ? 0.0 : bands[i];
    }
}

void getInternalAudioBuffer(int32_t** buffer) {
    *buffer = audioBuffer;
}
