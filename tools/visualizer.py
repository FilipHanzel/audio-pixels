from __future__ import annotations

import argparse
import math
import os
import typing
from textwrap import dedent
from typing import Iterable

os.environ["PYGAME_HIDE_SUPPORT_PROMPT"] = "1"

import numpy as np
import pygame as pg
import sounddevice as sd
from scipy.signal import get_window

if typing.TYPE_CHECKING:
    from _cffi_backend import _CDataBase
else:
    _CDataBase = None


class BufferedInputStream(sd.InputStream):
    """
    Wrapper around InputStream.

    Allows to access the most recent audio data outside of the callback method.
    Keeping the callback as simple as possible prevents InputStream from lagging.
    """

    def __init__(
        self,
        samplerate: int,
        blocksize: int,
        device: int | str,
        **kwargs,
    ):

        assert "callback" not in kwargs, "Cannot use custom callback."

        super().__init__(
            samplerate=samplerate,
            blocksize=blocksize,
            device=device,
            callback=self._callback,
            **kwargs,
        )

        buffer_shape = (
            self.blocksize if self.channels == 1 else (self.blocksize, self.channels)
        )
        self._buffer = np.zeros(buffer_shape, self.dtype)

    def _callback(
        self,
        indata: np.ndarray,
        frames: int,
        time: _CDataBase,
        status: sd.CallbackFlags,
    ) -> None:
        """Update audio buffer."""
        self._buffer = indata.reshape(self._buffer.shape)

    def read(self) -> np.ndarray:
        """Get a copy of a recent audio data."""
        return np.array(self._buffer, copy=True)


class Display:
    """
    Sound processing visualizer.

    ┌───────────────┬────────────────┐
    │               │                │
    │ RAW WAVEFORM  │                │
    │               │                │
    ├───────────────┤                │
    │               │    BUCKETED    │
    │               │                │
    │      FFT      │                │
    │               │                │
    │               │                │
    └───────────────┴────────────────┘
    """

    WINDOW_WIDTH = 1920
    WINDOW_HEIGHT = 1080

    MARGIN = 5

    COLOR_DARK = "#292623"
    COLOR_LIGHT = "#aaabac"
    COLOR_DIVIDER = "#555555"

    @staticmethod
    def _get_band_width(
        plot_width: int | float,
        plot_margin: int | float,
        n_bands: int,
        gap_width: int | float,
    ) -> float:
        return (plot_width - 2 * plot_margin - (n_bands - 1) * gap_width) / n_bands

    def __init__(
        self,
        block_size: int,
        n_bins: int,
    ):

        self.col_width = [
            int(self.WINDOW_WIDTH * 2 / 5),
            int(self.WINDOW_WIDTH * 3 / 5),
        ]
        self.row_height = [
            int(self.WINDOW_HEIGHT * 1 / 3),
            int(self.WINDOW_HEIGHT * 2 / 3),
        ]

        self.raw_band_gap = 0
        self.raw_band_width = self._get_band_width(
            plot_width=self.col_width[0],
            plot_margin=self.MARGIN,
            n_bands=block_size,
            gap_width=self.raw_band_gap,
        )
        self.raw_band_width_rounded = max(1, math.ceil(self.raw_band_width))
        self.raw_band_scale = self.row_height[0]

        self.fft_band_gap = 0
        self.fft_band_width = self._get_band_width(
            plot_width=self.col_width[0],
            plot_margin=self.MARGIN,
            n_bands=block_size // 2,
            gap_width=self.fft_band_gap,
        )
        self.fft_band_width_rounded = max(1, math.ceil(self.fft_band_width))
        self.fft_band_scale = self.row_height[1] / 100

        self.bucket_band_gap = 3
        self.bucket_band_width = self._get_band_width(
            plot_width=self.col_width[1],
            plot_margin=self.MARGIN,
            n_bands=n_bins,
            gap_width=self.bucket_band_gap,
        )
        self.bucket_band_width_rounded = max(1, math.ceil(self.bucket_band_width))
        self.bucket_band_scale = self.WINDOW_HEIGHT / 20

        pg.init()

        self.pg_screen = pg.display.set_mode(
            size=(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
        )
        self.pg_clock = pg.time.Clock()

    def draw(self, raw: Iterable, fft: Iterable, bands: Iterable) -> None:
        self.pg_screen.fill("#292623")

        for i, reading in enumerate(raw):
            x_offset = self.MARGIN + self.raw_band_width / 2
            x_offset += (self.raw_band_width + self.raw_band_gap) * i
            y_offset = self.row_height[0] / 2

            pg.draw.line(
                self.pg_screen,
                "#aaabac",
                (x_offset, y_offset),
                (x_offset, y_offset - reading * self.raw_band_scale),
                width=self.raw_band_width_rounded,
            )

        for i, amplitude in enumerate(fft):
            x_offset = self.MARGIN + self.fft_band_width / 2
            x_offset += (self.fft_band_width + self.fft_band_gap) * i
            y_offset = self.row_height[0] + self.row_height[1] - self.MARGIN

            pg.draw.line(
                self.pg_screen,
                "#aaabac",
                (x_offset, y_offset),
                (x_offset, y_offset - amplitude * self.fft_band_scale),
                width=self.fft_band_width_rounded,
            )

        for i, band in enumerate(bands):
            x_offset = self.col_width[0] + self.MARGIN + self.bucket_band_width / 2
            x_offset += (self.bucket_band_width + self.bucket_band_gap) * i
            y_offset = self.row_height[0] + self.row_height[1] - self.MARGIN

            pg.draw.line(
                self.pg_screen,
                "#aaabac",
                (x_offset, y_offset),
                (x_offset, y_offset - band * self.bucket_band_scale),
                width=self.bucket_band_width_rounded,
            )

    def flip(self):
        pg.display.flip()

    def tick(self, framerate: float = 0):
        self.pg_clock.tick(framerate)

    def quit(self):
        pg.quit()


# Methods to automatically divide frequency spectrum into bins


# def freq_to_mel(frequency: float) -> float:
#     return 2595 * math.log10(1 + frequency / 700)


# def mel_to_freq(mel: float) -> float:
#     return 700 * (10 ** (mel / 2595) - 1)


# def get_mel_thresholds(max_frequency: float, n_bins: int) -> float:
#     """
#     Calculate frequency thresholds that divide max_frequency into n_bins based on Mel scale.
#     """
#     mel_step = freq_to_mel(max_frequency) / n_bins
#     thresholds = [mel_to_freq(mel_step * i) for i in range(1, n_bins + 1)]
#     return thresholds


def freq_to_bark(frequency: float) -> float:
    return 6 * math.asinh(frequency / 600)


def bark_to_freq(bark: float) -> float:
    return 600 * math.sinh(bark / 6)


def get_bark_thresholds(max_frequency: float, n_bins: int) -> float:
    """
    Calculate frequency thresholds that divide max_frequency into n_bins based on Bark scale.
    """
    bark_step = freq_to_bark(max_frequency) / n_bins
    thresholds = [bark_to_freq(bark_step * i) for i in range(1, n_bins + 1)]
    return thresholds


def get_rfft_frequencies(n: int, sampling_rate: int) -> list[float]:
    """Get a list of n // 2 - 1 frequency values matching rfft result."""
    return [i * sampling_rate / n for i in range(n // 2)]


if __name__ == "__main__":
    description = dedent(
        """\
        Audio visualizer, for rapid prototyping.

        Example usage:
        > python visualizer.py list
        > python visualizer.py run --block-size 2048 --n-bins 64
        > python visualizer.py run --block-size 2048 --n-bins 32 --calibrate
        """
    )

    parser = argparse.ArgumentParser(description=description)
    subparsers = parser.add_subparsers(title="commands", required=True, dest="command")

    list_parser = subparsers.add_parser("list", help="List available input devices.")

    run_parser = subparsers.add_parser("run", help="Run visualizations.")
    run_parser.add_argument(
        "--device",
        dest="device",
        default=None,
        help="Audio input device to use.",
    )
    run_parser.add_argument(
        "--sample-rate",
        dest="sample_rate",
        type=int,
        default=44100,
    )
    run_parser.add_argument(
        "--block-size",
        dest="block_size",
        type=int,
        default=2048,
    )
    run_parser.add_argument(
        "--n-bins",
        dest="n_bins",
        type=int,
        choices=[16, 24, 32, 64],
        default=16,
    )
    run_parser.add_argument(
        "--calibration",
        dest="calibration",
        action="store_true",
        default=False,
    )

    args = parser.parse_args()

    if args.command == "list":

        in_devices = [d for d in sd.query_devices() if d["max_input_channels"] > 0]
        print(sd.DeviceList(in_devices))
        exit()

    if args.command == "run":
        if args.device is None:
            device = sd.query_devices(kind="input")["index"]
        elif args.device.isdigit():
            device = int(args.device)
        else:
            device = args.device

        audio_stream = BufferedInputStream(
            samplerate=args.sample_rate,
            blocksize=args.block_size,
            device=device,
            channels=1,
        )

        display = Display(args.block_size, args.n_bins)

        # Audio processing params

        window = get_window("hann", args.block_size, fftbins=True)

        frequencies = get_rfft_frequencies(args.block_size, args.sample_rate)
        band_cutoff_table = get_bark_thresholds(args.sample_rate // 2, args.n_bins)

        band_scale = 0.05

        if args.calibration:
            calibration_table = [0.0] * args.n_bins
            calibration_max = 0.0
            calibration_counter = 0

        # This was calibrated with pink noise and should work nicely,
        # but there is way too much noise for the signal to be usable.
        # TODO: Fix the noise and recalibrate.
        elif args.n_bins == 16:
            calibration_table = [1.0, 1.579, 2.967, 3.934, 10.496, 4.529, 2.819, 2.025, 1.288, 1.192, 3.972, 5.725, 4.957, 2.465, 4.515, 14.452]
        elif args.n_bins == 24:
            calibration_table = [1.0, 1.656, 2.004, 3.445, 4.077, 5.207, 11.74, 8.4, 4.891, 3.901, 2.711, 2.33, 1.606, 1.379, 1.445, 3.941, 6.296, 7.645, 7.734, 3.885, 2.587, 4.106, 16.077, 19.769]
        elif args.n_bins == 32:
            calibration_table = [2.357, 1.0, 2.06, 2.637, 3.228, 6.028, 4.95, 6.828, 14.251, 16.951, 7.569, 5.106, 5.386, 3.108, 2.807, 2.837, 2.006, 1.685, 1.509, 1.919, 4.588, 8.741, 6.26, 9.923, 9.482, 5.441, 4.287, 3.038, 4.502, 12.644, 24.705, 25.828]
        elif args.n_bins == 64:
            calibration_table = [6.206, 2.061, 1.0, 2.257, 3.613, 2.237, 2.94, 3.938, 5.363, 3.881, 7.85, 8.147, 6.339, 7.596, 8.291, 9.72, 15.826, 26.609, 19.087, 26.361, 11.688, 11.138, 6.986, 6.954, 7.405, 7.0, 4.609, 4.375, 4.466, 3.699, 4.942, 3.227, 3.078, 2.692, 2.332, 2.231, 2.392, 1.977, 1.894, 3.657, 4.934, 5.77, 9.595, 11.726, 10.446, 10.29, 12.838, 17.665, 18.469, 11.347, 10.365, 5.917, 7.247, 3.903, 3.406, 6.314, 4.95, 6.87, 14.149, 24.256, 29.694, 33.454, 33.345, 32.136]
        else:
            raise NotImplementedError()
        
        with audio_stream:
            running = True
            while running:
                for event in pg.event.get():
                    if event.type == pg.QUIT:
                        running = False

                sample = audio_stream.read()

                fft = sample * window
                fft = np.fft.rfft(sample)
                fft = fft[1:]
                fft = np.sqrt(fft.real**2 + fft.imag**2)

                bands = [0] * args.n_bins
                band_idx = 0
                for i in range(args.block_size // 2):
                    if band_cutoff_table[band_idx] < frequencies[i]:
                        band_idx += 1
                    bands[band_idx] += fft[i]

                for i in range(args.n_bins):
                    bands[i] *= band_scale

                if args.calibration:
                    calibration_counter += 1

                    for i in range(args.n_bins):
                        calibration_table[i] += float(bands[i])

                    if calibration_counter >= 512:
                        m = max(calibration_table)
                        for i in range(args.n_bins):
                            calibration_table[i] = m / calibration_table[i]
                        
                        print(f"Calibration table: {[round(v, 3) for v in calibration_table]}")
                        
                        calibration_counter = 0
                        calibration_table = [0.0] * args.n_bins

                else:
                    for i in range(args.n_bins):
                        bands[i] *= calibration_table[i]

                display.draw(sample, fft, bands)
                display.flip()
                display.tick()

            display.quit()
