#ifndef VISUALIZATION_H
#define VISUALIZATION_H

// LED matrix configuration
#define LED_MATRIX_DATA_PIN   26 //
#define LED_MATRIX_N_BANDS    16 // Number of columns or bands
#define LED_MATRIX_N_PER_BAND 23 // Number of rows or LEDs per band
#define LED_MATRIX_N          (LED_MATRIX_N_BANDS * LED_MATRIX_N_PER_BAND)

/**
 * @brief Enum-like definition for selecting visualization type.
 */
typedef int VisualizationType;
#define VISUALIZATION_TYPE_NONE      -1
#define VISUALIZATION_TYPE_BARS      0
#define VISUALIZATION_TYPE_SPECTRUM  1
#define VISUALIZATION_TYPE_FIRE      2
#define VISUALIZATION_TYPE_MAX_VALUE 2

/**
 * @brief Enum-like definition for selecting visualization color palette.
 */
typedef int VisualizationPalette;
#define VISUALIZATION_PALETTE_NONE -1

#define VISUALIZATION_PALETTE_BARS_WARM      0
#define VISUALIZATION_PALETTE_BARS_OCEAN     1
#define VISUALIZATION_PALETTE_BARS_FUNKY     2
#define VISUALIZATION_PALETTE_BARS_MAX_VALUE 2

#define VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_GREEN 0
#define VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_BLUE  1
#define VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_RED   2
#define VISUALIZATION_PALETTE_SPECTRUM_HEATMAP_PINK  3
#define VISUALIZATION_PALETTE_SPECTRUM_MAX_VALUE     3

#define VISUALIZATION_PALETTE_FIRE_RED       0
#define VISUALIZATION_PALETTE_FIRE_BLUE      1
#define VISUALIZATION_PALETTE_FIRE_GREEN     2
#define VISUALIZATION_PALETTE_FIRE_MAX_VALUE 2

/**
 * @brief Initializes the LED strip and prepares it for use.
 *
 * This function configures the LED strip by associating the `leds` array with the FastLED library.
 *
 * @note This function must be called before any other LED control functions.
 */
void setupLedStrip();

/**
 * @brief Initializes the specified LED visualization.
 *
 * This function sets up the LED visualization. It ensures that only one visualization is active.
 *
 * @param visualization The type of visualization to be set up.
 *
 * @note This function should only be called when no other visualization is currently active.
 */
void setupVisualization(VisualizationType visualization);

/**
 * @brief Sets the color palette for the active LED visualization.
 *
 * This function assigns the specified `palette` to the currently active visualization.
 * It ensures that a visualization has already been set up before applying the palette.
 *
 * @param palette The color palette to be applied to the active visualization.
 *
 * @note Visualization should be set up using `setupVisualization` before calling this function.
 */
void setVisualizationPalette(VisualizationPalette palette);

/**
 * @brief Deactivates the current LED visualization and resets internal state.
 *
 * This function deactivates the current visualization and clears all the buffers. It should be
 * called when the visualization is no longer needed or before initializing a different visualization,
 * as only one visualization can be active at a time.
 *
 * @note This function can only be called if a visualization is currently active.
 */
void teardownVisualization();

/**
 * @brief Updates the active LED visualization based on the current visualization type.
 *
 * @param bands Array of floating-point values to update the visualization with.
 *
 * @note Ensure that a visualization is properly set up before calling this function.
 */
void updateVisualization(float *bars);

/**
 * @brief Displays the current LED data on the matrix using FastLED.show().
 */
void showVisualization();

#endif
