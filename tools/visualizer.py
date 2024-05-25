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

        if args.calibration:
            calibration_table = [1.0] * args.n_bins
            calibration_bands = [0.0] * args.n_bins
        # fmt: off
        elif args.n_bins == 16:
            calibration_table = [73.637, 36.38, 32.924, 30.736, 29.887, 32.362, 34.04, 35.583, 37.915, 43.975, 46.279, 53.236, 66.515, 64.834, 82.786, 95.451]
        elif args.n_bins == 24:
            calibration_table = [69.305, 30.524, 26.926, 23.88, 23.146, 22.315, 20.204, 24.864, 21.131, 22.508, 26.321, 25.768, 24.164, 28.813, 31.874, 31.457, 31.532, 39.849, 45.721, 42.973, 44.885, 52.768, 64.966, 64.99]
        elif args.n_bins == 32:
            calibration_table = [34.108, 29.789, 25.454, 21.266, 21.764, 15.325, 18.832, 16.803, 17.262, 18.131, 17.015, 18.065, 18.347, 20.636, 20.308, 20.143, 19.847, 21.859, 23.471, 26.745, 24.463, 24.067, 25.957, 30.151, 34.825, 34.191, 31.721, 36.077, 41.139, 45.742, 50.584, 50.985]
        elif args.n_bins == 64:
            calibration_table = [41.08, 23.914, 19.405, 17.252, 18.686, 13.431, 14.971, 10.018, 9.437, 14.739, 8.713, 8.722, 13.088, 10.028, 9.261, 9.054, 9.705, 9.907, 14.005, 10.228, 11.11, 9.745, 10.184, 10.235, 10.644, 9.895, 12.009, 10.766, 10.458, 12.391, 11.262, 12.901, 12.207, 10.708, 10.418, 12.524, 13.266, 13.216, 14.934, 13.575, 14.367, 13.279, 13.008, 12.723, 13.251, 14.235, 16.326, 17.129, 20.103, 19.173, 17.684, 17.44, 15.978, 17.537, 18.818, 19.388, 19.886, 22.151, 22.786, 24.076, 26.023, 26.508, 26.393, 28.321]
        else:
            raise NotImplementedError()
        # fmt: on

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

                if args.calibration:
                    for i in range(args.n_bins):
                        if bands[i] > calibration_bands[i]:
                            calibration_bands[i] = round(bands[i], 3)
                    print(calibration_bands)
                else:
                    for i in range(args.n_bins):
                        bands[i] /= calibration_table[i]

                display.draw(sample, fft, bands)
                display.flip()
                display.tick()

            display.quit()
