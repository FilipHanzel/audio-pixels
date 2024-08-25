# Audio Pixels
**Work-in-progress** prototype of an LED matrix audio visualizer based on ESP32 microcontroller. The goal is to create visual display that reacts dynamically to audio input, producing real-time visualizations on an LED matrix.

## Current status
The project is still under development but is already a working prototype. The hardware setup, including the LED display, will be iterated upon as the project progresses, so the duct tape will eventually go away.

<p align="center"><img width="600" src="https://github.com/FilipHanzel/audio-pixels/blob/1e1b17913fbbf162170b985329f6e1fc7da86b8f/imgs/visualizations/bars.gif"></p>
<p align="center"><img width="600" src="https://github.com/FilipHanzel/audio-pixels/blob/1e1b17913fbbf162170b985329f6e1fc7da86b8f/imgs/visualizations/spectrum.gif"></p>
<p align="center"><img width="600" src="https://github.com/FilipHanzel/audio-pixels/blob/1e1b17913fbbf162170b985329f6e1fc7da86b8f/imgs/visualizations/fire.gif"></p>

### Board
I'm currently using the INMP441 for microphone input and the built-in ADC for line-in. A logic level shifter is used for communication with the LEDs.
Recently, I added RS485 boards after the logic level converter for LED communication, hoping to reduce noise on the internal ADC, but it didn't work as expected.
Since the wire from controller to LED strip is only about 2m long, I might remove RS485 boards to simplify the design. To reduce noise on line-in input,
I'm planning to use external ADCs.

<p align="center"><img src="https://github.com/FilipHanzel/audio-pixels/blob/1e1b17913fbbf162170b985329f6e1fc7da86b8f/imgs/board.jpg"></p>
