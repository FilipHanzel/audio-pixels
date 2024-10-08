#include "audio.h"

#include <Arduino.h>
#include <cstddef>
#include <driver/i2s.h>
#include <esp_dsp.h>
#include <math.h>
#include <string.h>

#define DEBUG

#include "macros.h"

// clang-format off

// This calibration was done with nothing plugged in, which is when the noise is at its loudest.
// There is some potential to adjust the board design to reduce noise.
__attribute__((aligned(16))) static float noiseTableLineIn[AUDIO_N_BANDS] = {
    466857.09, 346476.28, 168687.16, 110188.34, 102911.09,  51135.57,  52786.84,  33903.78,
     64754.73,  45940.25,  50925.71,  49469.94,  51538.77,  51929.79,  41975.98,  47666.91,
     35906.05,  37451.16,  38529.77,  38729.49,  36585.22,  40265.46,  44595.30,  42013.20,
     45790.86,  51991.13,  57315.60,  71747.27, 108651.09, 205939.69, 856251.38, 138647.66,
};

__attribute__((aligned(16))) static float calibrationTableLineIn[AUDIO_N_BANDS] = {
    1.00, 1.57, 2.08, 2.84, 3.59, 3.92, 4.76, 5.71,
    3.37, 3.70, 4.31, 3.70, 4.31, 3.53, 4.22, 3.65,
    4.50, 4.11, 3.73, 3.96, 4.35, 3.80, 3.65, 4.18,
    4.57, 3.85, 3.82, 4.28, 4.13, 3.69, 3.65, 5.19,
};

__attribute__((aligned(16))) static float noiseTableMic[AUDIO_N_BANDS] = {
    22507.86, 23126.12, 34750.77, 51859.29, 73113.01, 76401.06,  66875.94, 60780.73,
    80576.64, 48133.16, 29567.83, 30516.37, 23424.55, 19969.64,  16416.67, 23410.05,
    18108.18, 15786.40, 17565.38, 20108.31, 21119.27, 21046.46,  28456.55, 31818.38,
    29095.50, 43041.91, 39937.23, 50131.61, 56281.11, 65008.17, 250702.50, 50898.48,
};

__attribute__((aligned(16))) static float calibrationTableMic[AUDIO_N_BANDS] = {
    4.54, 2.02, 1.95, 2.26, 2.18, 2.26,  2.26,  2.62,
    1.44, 1.71, 2.14, 1.22, 1.13, 1.29,  1.28,  1.00,
    1.83, 1.32, 1.14, 1.32, 2.33, 2.02,  1.66,  1.54,
    1.91, 4.62, 2.65, 2.68, 5.58, 2.84, 11.96, 19.37,
};

__attribute__((aligned(16))) static float noiseTableNone[AUDIO_N_BANDS] = {
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
};

__attribute__((aligned(16))) static float calibrationTableNone[AUDIO_N_BANDS] = {
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
};

// clang-format on

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

    // Windowing helps reduce frequency leakage between bands but can cause some parts of
    // short signals to be lost, especially if they start near the edge of the audio sample.
    // This means the same short signal might result in different responses. To reduce this problem,
    // the shape of the window is made closer to a 'square' by taking the square root of the values
    // several times. This keeps more of the signal intact while still reducing leakage.
    dsps_wind_blackman_harris_f32(window, AUDIO_N_SAMPLES);
    for (int i = 0; i < AUDIO_N_SAMPLES; i++) {
        window[i] = sqrtf(window[i]);
        window[i] = sqrtf(window[i]);
        window[i] = sqrtf(window[i]);
    }
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
        bands[bandIdx] += fftBuffer[i];

        float frequency = i * AUDIO_SAMPLING_RATE / AUDIO_N_SAMPLES;
        if (frequencyThresholds[bandIdx] < frequency) {
            bandIdx++;

            if (bandIdx > AUDIO_N_BANDS) {
                PRINTF("Frequency band grouping error. Halt!\n");
                while (true) continue;
            }
        }
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
    // Scaling up should be quicker than scaling down (AUDIO_BAND_SCALE_UP_FACTOR < AUDIO_BAND_SCALE_DOWN_FACTOR),
    // but it shouldn't scale all the way up to the maximum value, allowing the bands to remain high for a while.
    if (max > bandScale) {
        bandScale = (max * 0.85 + bandScale * (AUDIO_BAND_SCALE_UP_FACTOR - 1)) / AUDIO_BAND_SCALE_UP_FACTOR;
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
