#include "audio.h"

#include <Arduino.h>
#include <cstddef>
#include <driver/i2s.h>
#include <esp_dsp.h>
#include <math.h>
#include <string.h>

#define DEBUG

#include "macros.h"

// This calibration was done with nothing plugged in, which is when the noise is at its loudest.
// There is some potential to adjust the board design to reduce noise, but current noise level is acceptable.
__attribute__((aligned(16))) static float noiseTableLineIn[AUDIO_N_BANDS] = {
    293809.03,
    443322.09,
    134940.98,
    80323.16,
    103227.42,
    79087.91,
    78232.05,
    68127.78,
    61421.70,
    58895.67,
    67339.95,
    85825.51,
    105689.73,
    147244.81,
    307407.16,
    947407.88,
};

// This calibration was performed using custom noise, somewhere between pink and brown.
// TODO: Create a script to generate proper calibration noise.
__attribute__((aligned(16))) static float calibrationTableLineIn[AUDIO_N_BANDS] = {
    1.43,
    1.39,
    2.88,
    3.81,
    2.72,
    2.91,
    2.89,
    2.65,
    2.79,
    2.34,
    2.22,
    1.89,
    1.46,
    1.21,
    1.00,
    1.03,
};

__attribute__((aligned(16))) static float noiseTableMic[AUDIO_N_BANDS] = {
    36412.93,
    48928.23,
    73170.5,
    73419.59,
    75094.67,
    37057.88,
    32166.15,
    25044.71,
    24497.23,
    28754.62,
    34169.77,
    41103.89,
    49424.59,
    65262.34,
    85037.69,
    194271.53,
};

// This is a copy of calibration values for line-in input.
// TODO: Recalibrate properly.
__attribute__((aligned(16))) static float calibrationTableMic[AUDIO_N_BANDS] = {
    1.43,
    1.39,
    2.88,
    3.81,
    2.72,
    2.91,
    2.89,
    2.65,
    2.79,
    2.34,
    2.22,
    1.89,
    1.46,
    1.21,
    1.00,
    1.03,
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

__attribute__((aligned(16))) static int32_t audioBuffer[AUDIO_N_SAMPLES * 2] = {0};
__attribute__((aligned(16))) static float window[AUDIO_N_SAMPLES];
__attribute__((aligned(16))) static float fftBuffer[AUDIO_N_SAMPLES * 2];
__attribute__((aligned(16))) static float frequencyThresholds[AUDIO_N_BANDS] = {0};

static float bandScale = 0.0;

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

    dsps_wind_hann_f32(window, AUDIO_N_SAMPLES);
}

static void setupMic() {
    const i2s_driver_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLING_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
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
        .bck_io_num = AUDIO_MIC_BIT_CLOCK_PIN,
        .ws_io_num = AUDIO_MIC_LR_SELECT_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = AUDIO_MIC_DATA_PIN,
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
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLING_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = AUDIO_N_SAMPLES,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 512 * AUDIO_SAMPLING_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
    };

    const i2s_pin_config_t i2sPinConfig = {
        .mck_io_num = AUDIO_LINE_IN_MASTER_CLOCK_PIN,
        .bck_io_num = AUDIO_LINE_IN_BIT_CLOCK_PIN,
        .ws_io_num = AUDIO_LINE_IN_LR_SELECT_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = AUDIO_LINE_IN_DATA_PIN,
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

void teardownAudioSource() {
    if (currentAudioSource == AUDIO_SOURCE_NONE) {
        PRINTF("Audio source is not set up. Halt!\n");
        while (true) continue;
    }

    esp_err_t err = i2s_driver_uninstall(AUDIO_I2S_PORT);
    if (err != ESP_OK) {
        PRINTF("Error uninstalling I2S driver: 0x(%x). Halt!\n", err);
        while (true) continue;
    }

    currentAudioSource = AUDIO_SOURCE_NONE;
}

void readAudioDataToBuffer() {
    size_t bytesRead;
    i2s_read(AUDIO_I2S_PORT, audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);

    // The raw audio samples are stored in the most significant bytes, so we need to shift them right
    // to obtain the actual values. For both INMP441 mic and PCM1808 ADC, each sample is 24 bits,
    // so we shift by at least 8 bits + some more to reduce noise.
    for (int i = 0; i < AUDIO_N_SAMPLES * 2; i++) {
        audioBuffer[i] >>= 12;
    }

    float avg = 0.0;
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        // Stereo to mono conversion
        audioBuffer[i] = audioBuffer[i * 2] + audioBuffer[i * 2 + 1];

        avg += audioBuffer[i];
    }
    avg /= AUDIO_N_SAMPLES;
    // Normalization by subtracting average signal
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

void resetAudioBandScale(AudioSource audioSource) {
    if (audioSource == AUDIO_SOURCE_MIC) {
        bandScale = AUDIO_DEFAULT_BAND_SCALE_MIC;
    } else {
        bandScale = AUDIO_DEFAULT_BAND_SCALE_LINE_IN;
    }
}

void processAudioData(float *bands) {
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        fftBuffer[i * 2 + 0] = audioBuffer[i] * window[i];
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

void scaleAudioData(float *bands) {
    float max = 0.0;
    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        max = max < bands[i] ? bands[i] : max;
    }
    if (max > bandScale) {
        bandScale = (max + bandScale * (AUDIO_BAND_SCALE_UP_FACTOR - 1)) / AUDIO_BAND_SCALE_UP_FACTOR;
    } else {
        bandScale = (max + bandScale * (AUDIO_BAND_SCALE_DOWN_FACTOR - 1)) / AUDIO_BAND_SCALE_DOWN_FACTOR;
    }
    bandScale = bandScale < 1.0 ? 1.0 : bandScale;
    for (int i = 0; i < AUDIO_N_BANDS; i++) {
        bands[i] /= bandScale * 0.95;
        bands[i] = bands[i] > 1.0 ? 1.0 : bands[i];
    }
}

void getInternalAudioBuffer(int32_t **buffer) {
    *buffer = audioBuffer;
}
