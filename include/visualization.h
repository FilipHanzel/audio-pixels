#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#define LED_MATRIX_DATA_PIN   5
#define LED_MATRIX_N_BANDS    16
#define LED_MATRIX_N_PER_BAND 23
#define LED_MATRIX_N          (LED_MATRIX_N_BANDS * LED_MATRIX_N_PER_BAND)

typedef int VisualizationType;
#define VISUALIZATION_TYPE_NONE      -1
#define VISUALIZATION_TYPE_BARS      0
#define VISUALIZATION_TYPE_SPECTRUM  1
#define VISUALIZATION_TYPE_FIRE      2
#define VISUALIZATION_TYPE_MAX_VALUE 2

typedef int VisualizationPalette;
#define VISUALIZATION_PALETTE_NONE -1

#define VISUALIZATION_PALETTE_BARS_WARM      0
#define VISUALIZATION_PALETTE_BARS_GREEN     1
#define VISUALIZATION_PALETTE_BARS_BLUE      2
#define VISUALIZATION_PALETTE_BARS_MAX_VALUE 2

#define VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_GREEN 0
#define VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_BLUE  1
#define VISUALIZATION_PALETTE_SPECTRUM_MAGMA_PINK    2
#define VISUALIZATION_PALETTE_SPECTRUM_MAX_VALUE     2

#define VISUALIZATION_PALETTE_FIRE_RED       0
#define VISUALIZATION_PALETTE_FIRE_BLUE      1
#define VISUALIZATION_PALETTE_FIRE_GREEN     2
#define VISUALIZATION_PALETTE_FIRE_MAX_VALUE 2

void setupLedStrip();
void setupVisualization(VisualizationType visualization);
void setVisualizationPalette(VisualizationPalette palette);
void teardownVisualization(VisualizationType visualization);
void updateVisualization(float *bars);

#endif
