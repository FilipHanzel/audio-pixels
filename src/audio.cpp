#include "audio.h"

#include <driver/i2s.h>
#include <esp_dsp.h>
#include <math.h>
#include <string.h>

#include <cstddef>

#include "macros.h"

__attribute__((aligned(16))) static float noiseTableLineIn[AUDIO_N_BANDS] = {
    1173.29,
    1404.40,
    1589.53,
    2361.81,
    1707.81,
    3703.57,
    6061.60,
    5411.36,
    14170.02,
    7748.10,
    11478.27,
    15091.23,
    22618.30,
    31175.87,
    31952.33,
    53590.45,
};

__attribute__((aligned(16))) static float calibrationTableLineIn[AUDIO_N_BANDS] = {
    5.15,
    4.23,
    6.04,
    7.00,
    4.19,
    4.16,
    3.67,
    3.13,
    3.16,
    2.23,
    1.99,
    1.71,
    1.45,
    1.24,
    1.01,
    1.00,
};

__attribute__((aligned(16))) static float noiseTableMic[AUDIO_N_BANDS] = {
    103514.21,
    143685.81,
    117731.21,
    52289.12,
    40885.44,
    34046.66,
    31747.25,
    30486.73,
    36641.52,
    37333.33,
    44708.25,
    52569.50,
    67231.85,
    79593.04,
    145475.98,
    234601.82,
};

__attribute__((aligned(16))) static float calibrationTableMic[AUDIO_N_BANDS] = {
    1.00,
    1.43,
    2.78,
    4.96,
    7.45,
    7.52,
    2.21,
    2.68,
    2.61,
    2.05,
    1.16,
    3.28,
    2.01,
    1.57,
    2.31,
    3.23,
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
static float *currentNoiseTable = noiseTableNone;
static float *currentCalibrationTable = calibrationTableNone;

__attribute__((aligned(16))) static int32_t audioBuffer[AUDIO_N_SAMPLES] = {0};
__attribute__((aligned(16))) static float fftBuffer[AUDIO_N_SAMPLES * 2];
__attribute__((aligned(16))) static float frequencyThresholds[AUDIO_N_BANDS] = {0};

void setupAudioProcessing() {
    // Frequency thresholds are based on a modified Bark scale.
    // To better suit audio visualization needs, higher frequencies
    // are compressed into fewer bands, as they are ususlly not the
    // key components of audio signal.
    float step = (6.0 + 1.7) * asinh(AUDIO_SAMPLING_RATE / 2.0 / 600.0) / AUDIO_N_BANDS;
    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        frequencyThresholds[i] = 600.0 / 3.3 * sinh(step * (i + 1) / 6.0);
    }

    esp_err_t err = dsps_fft2r_init_fc32(NULL, AUDIO_N_SAMPLES);
    if (err != ESP_OK) {
        PRINTF("Not possible to initialize FFT2R. Error: 0x(%x). Halt!\n", err);
        while (true) continue;
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
        while (true) continue;
    }

    err = i2s_set_pin(AUDIO_I2S_PORT, &i2sPinConfig);
    if (err != ESP_OK) {
        PRINTF("Error setting I2S pin: 0x(%x). Halt!\n", err);
        while (true) continue;
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
        while (true) continue;
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, AUDIO_LINE_IN_PIN);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC mode: 0x(%x). Halt!\n", err);
        while (true) continue;
    }

    err = adc1_config_channel_atten(AUDIO_LINE_IN_PIN, ADC_ATTEN_DB_12);
    if (err != ESP_OK) {
        PRINTF("Error setting up ADC attenuation: 0x(%x). Halt!\n", err);
        while (true) continue;
    }

    err = i2s_adc_enable(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error enabling ADC: 0x(%x). Halt!\n", err);
        while (true) continue;
    }
}

void setupAudioSource(AudioSource audioSource) {
    if (currentAudioSource != AUDIO_SOURCE_NONE) {
        PRINTF("Audio source already set up. Halt!\n");
        while (true) continue;
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
        while (true) continue;
    }
}

static void teardownLineIn() {
    esp_err_t err = i2s_adc_disable(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error disabling ADC: 0x(%x). Halt!\n", err);
        while (true) continue;
    }

    err = i2s_driver_uninstall(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error uninstalling I2S driver: 0x(%x). Halt!\n", err);
        while (true) continue;
    }

    // Without this, switching from line-in to mic won't work.
    // I found the function when I was looking for something
    // to reset the ADC and/or I2S module.
    err = adc_set_i2s_data_source(ADC_I2S_DATA_SRC_IO_SIG);
    if (err != ESP_OK) {
        PRINTF("Error uninstalling I2S driver: 0x(%x). Halt!\n", err);
        while (true) continue;
    }
}

void teardownAudioSource() {
    if (currentAudioSource == AUDIO_SOURCE_NONE) {
        PRINTF("Audio source is not set up. Halt!\n");
        while (true) continue;
    }

    if (currentAudioSource == AUDIO_SOURCE_MIC) {
        teardownMic();
    } else if (currentAudioSource == AUDIO_SOURCE_LINE_IN) {
        teardownLineIn();
    } else {
        PRINTF("Unknown audio source. Halt!\n");
        while (true) continue;
    }

    currentAudioSource = AUDIO_SOURCE_NONE;
}

void readAudioDataToBuffer() {
    size_t bytesRead;
    i2s_read(AUDIO_I2S_PORT, audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);

    float avg = 0.0;
    // The raw audio samples are stored in the most significant bytes, so we need to shift them right
    // to obtain the actual values. For the INMP441 microphone, each sample is 24 bits, so we shift
    // by at least 8 bits + some more to reduce noise. For the line-in input, the ESP32's internal ADC
    // provides 12-bit resolution, stored across two bytes, so we shift by 16 bits.
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

void setupAudioNoiseTable(AudioSource audioSource) {
    if (audioSource == AUDIO_SOURCE_MIC) {
        currentNoiseTable = noiseTableMic;
    } else {
        currentNoiseTable = noiseTableLineIn;
    }
}

void setupAudioCalibrationTable(AudioSource audioSource) {
    if (audioSource == AUDIO_SOURCE_MIC) {
        currentCalibrationTable = calibrationTableMic;
    } else {
        currentCalibrationTable = calibrationTableLineIn;
    }
}

void setupAudioTables(AudioSource audioSource) {
    setupAudioNoiseTable(audioSource);
    setupAudioCalibrationTable(audioSource);
}

void processAudioData(float *bands) {
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        fftBuffer[i * 2 + 0] = audioBuffer[i];
        fftBuffer[i * 2 + 1] = 0;
    }

    esp_err_t err = dsps_fft2r_fc32(fftBuffer, AUDIO_N_SAMPLES);
    if (err != ESP_OK) {
        PRINTF("FFT2R error: 0x(%x). Halt!\n", err);
        while (true) continue;
    }
    err = dsps_bit_rev2r_fc32(fftBuffer, AUDIO_N_SAMPLES);
    if (err != ESP_OK) {
        PRINTF("FFT2R bit reverse error: 0x(%x). Halt!\n", err);
        while (true) continue;
    }

    // Compute power spectrum
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        fftBuffer[i] = sqrtf(fftBuffer[i * 2 + 0] * fftBuffer[i * 2 + 0] + fftBuffer[i * 2 + 1] * fftBuffer[i * 2 + 1]);
    }

    // Distribute power spectrum values into frequency bands
    memset(bands, 0, sizeof(float) * AUDIO_N_BANDS);
    int bandIdx = 0;
    for (int i = 1; i < AUDIO_N_SAMPLES / 2; i++) {
        float frequency = i * AUDIO_SAMPLING_RATE / AUDIO_N_SAMPLES;
        if (frequencyThresholds[bandIdx] < frequency) {
            bandIdx++;

            if (bandIdx > AUDIO_N_BANDS) {
                PRINTF("Frequency band grouping error. Halt!\n");
                while (true) continue;
            }
        }
        bands[bandIdx] += fftBuffer[i];
    }

    // Apply noise reduction and calibration to each frequency band
    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        bands[i] -= currentNoiseTable[i];
        bands[i] *= currentCalibrationTable[i];

        bands[i] = bands[i] < 0.0 ? 0.0 : bands[i];
    }
}

void getInternalAudioBuffer(int32_t **buffer) {
    *buffer = audioBuffer;
}
