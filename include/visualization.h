#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#define LED_MATRIX_DATA_PIN 5
#define LED_MATRIX_N_BANDS 16
#define LED_MATRIX_N_PER_BAND 23
#define LED_MATRIX_N (LED_MATRIX_N_BANDS * LED_MATRIX_N_PER_BAND)

typedef int Visualization;
#define VISUALIZATION_NONE -1
#define VISUALIZATION_RED_BARS 0
#define VISUALIZATION_GREEN_BARS 1
#define VISUALIZATION_BLUE_BARS 2
#define VISUALIZATION_TYPE_MAX_VALUE 2

void setupLedStrip();
void setupVisualization(Visualization visualization);
void teardownVisualization(Visualization visualization);
void updateVisualization(float *bars);

#endif
