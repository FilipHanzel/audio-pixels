#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>

// Audio sampling nad processing configuration
#define AUDIO_I2S_PORT      I2S_NUM_0 // I2S port for audio input
#define AUDIO_N_SAMPLES     1024      //
#define AUDIO_SAMPLING_RATE 44100     //
#define AUDIO_N_BANDS       16        // Number of frequency bands produced

// Pins for audio inputs
#define AUDIO_LINE_IN_PIN ADC1_CHANNEL_5 // ADC channel for line-in audio input
#define AUDIO_MIC_WS_PIN  21             // I2S word select (WS) pin for microphone input
#define AUDIO_MIC_SD_PIN  18             // I2S serial data (SD) pin for microphone input
#define AUDIO_MIC_SCK_PIN 19             // I2S serial clock (SCK) pin for microphone input

/**
 * @brief Enum-like definition for selecting audio sources.
 */
typedef int AudioSource;
#define AUDIO_SOURCE_NONE           -1
#define AUDIO_SOURCE_MIC            0
#define AUDIO_SOURCE_LINE_IN        1
#define AUDIO_SOURCE_TYPE_MAX_VALUE 1

/**
 * @brief Initializes specified audio source.
 *
 * This function sets up the necessary components to enable audio input from the specified source.
 * Only one audio source can be initialized at a time; to initialize a different source, you must
 * first call `teardownAudioSource` on any already initialized source.
 *
 * @param audioSource Audio source to set up.
 */
void setupAudioSource(AudioSource audioSource);

/**
 * @brief Tears down audio source, releasing any resources.
 *
 * This function deactivates the current audio source and frees associated resources. It should be
 * called when the audio source is no longer needed or before initializing a different source, as
 * only one audio source can be active at a time.
 */
void teardownAudioSource();

/**
 * @brief Reads audio data from the currently initialized audio source into an internal buffer.
 *
 * This function captures a batch of audio samples from the active audio source, writes it to internal
 * buffer and processes captured data by subtracting average to remove DC offset.
 *
 * @note Ensure that the correct audio source is initialized before calling this function.
 */
void readAudioDataToBuffer();

/**
 * @brief Configures the audio processing environment, including frequency thresholds and FFT initialization.
 *
 * @note This function must be called before any audio data processing can occur.
 */
void setupAudioProcessing();

/**
 * @brief Configures noise table for the specified audio source.
 *
 * @param audioSource The audio source for which to configure the noise table.
 */
void setupAudioNoiseTable(AudioSource audioSource);

/**
 * @brief Configures calibration table for the specified audio source.
 *
 * @param audioSource The audio source for which to configure the calibration table.
 */
void setupAudioCalibrationTable(AudioSource audioSource);

/**
 * @brief Configures noise and calibration tables for the specified audio source.
 *
 * @param audioSource The audio source for which to configure the tables.
 */
void setupAudioTables(AudioSource audioSource);

/**
 * @brief Processes the audio data to produce calibrated frequency band power levels.
 *
 * @param bands Pointer to an array where the result will be stored.
 *
 * @note The function operates on the internal `audioBuffer` and `fftBuffer` variables.
 *       It assumes that `fftBuffer` is initialized and `audioBuffer` is filled with the latest audio data.
 * @note The `currentNoiseTable` and `currentCalibrationTable` are used to correct the power levels.
 */
void processAudioData(float *bands);

/**
 * @brief Provides access to the internal audio buffer for debugging purposes.
 *
 * @param buffer A pointer to a pointer that will be set to the address of the internal `audioBuffer`.
 */
void getInternalAudioBuffer(int32_t **buffer);

#endif
