#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>

#define AUDIO_I2S_PORT I2S_NUM_0
#define AUDIO_N_SAMPLES 1024
#define AUDIO_SAMPLING_RATE 44100
#define AUDIO_N_BANDS 16

#define AUDIO_LINE_IN_PIN ADC1_CHANNEL_5
#define AUDIO_MIC_WS_PIN 21
#define AUDIO_MIC_SD_PIN 18
#define AUDIO_MIC_SCK_PIN 19

typedef int AudioSource;
#define AUDIO_SOURCE_NONE -1
#define AUDIO_SOURCE_MIC 0
#define AUDIO_SOURCE_LINE_IN 1

void setupAudioSource(AudioSource audioSource);
void teardownAudioSource(AudioSource audioSource);

void readAudioDataToBuffer();

void setupAudioProcessing();
void setupAudioTables(AudioSource audioSource);

void processAudioData(float* bands);

void getInternalAudioBuffer(int32_t** buffer);

#endif
