import argparse
from argparse import RawTextHelpFormatter
from itertools import zip_longest
from textwrap import dedent

import matplotlib.pyplot as plt
import serial
import serial.tools.list_ports
from matplotlib.animation import FuncAnimation


def numeric_or_string(value: str) -> float | str:
    try:
        return float(value)
    except Exception:
        return value


if __name__ == "__main__":
    description = dedent(
        """\
        Serial plotter, similar to the one in Arduino IDE.

        Example usage:
        > python plotter.py list
        > python plotter.py plot --port /dev/ttyUSB0 --rate 115200 --y-max 4096
        """
    )

    parser = argparse.ArgumentParser(description=description, formatter_class=RawTextHelpFormatter)
    subparsers = parser.add_subparsers(title="commands", required=True, dest="command")

    list_parser = subparsers.add_parser("list", help="List available serial ports.")

    plot_parser = subparsers.add_parser("plot", help="Plot serial input.")
    plot_parser.add_argument(
        "-p",
        "--port",
        dest="port",
        type=str,
        required=True,
        help="Serial port.",
    )
    plot_parser.add_argument(
        "-r",
        "--rate",
        dest="rate",
        type=int,
        required=True,
        help="Boudrate.",
    )
    plot_parser.add_argument(
        "--x-ticks",
        dest="x_ticks",
        type=int,
        default=1000,
        help="Maximum number of data points to plot at once.",
    )
    plot_parser.add_argument(
        "--y-min",
        dest="y_min",
        type=numeric_or_string,
        default="auto",
        help="Lower y-axis limit.",
    )
    plot_parser.add_argument(
        "--y-max",
        dest="y_max",
        type=numeric_or_string,
        default="auto",
        help="Upper y-axis limit.",
    )
    plot_parser.add_argument(
        "--width",
        dest="width",
        type=int,
        default=1600,
        help="Window width.",
    )
    plot_parser.add_argument(
        "--height",
        dest="height",
        type=int,
        default=900,
        help="Window height.",
    )
    plot_parser.add_argument(
        "--dpi",
        dest="dpi",
        type=int,
        default=100,
        help="Plot DPI.",
    )
    plot_parser.add_argument(
        "--drop-frames",
        dest="drop_frames",
        type=int,
        default=1,
        help=(
            "How many frames/lines to drop when reading from serial port. "
            "Helpful when data isn't read fast enough and decoding fails."
        )
    )

    args = parser.parse_args()

    if args.command == "list":
        response = ""
        for port in serial.tools.list_ports.comports():
            if response:
                response += "\n"
            response += f"device:      {port.device}\n"
            response += f"name:        {port.name}\n"
            response += f"description: {port.description}\n"
            response += f"hwid:        {port.hwid}\n"
        print(response)

    elif args.command == "plot":
        if isinstance(args.y_min, str) and args.y_min != "auto":
            raise ValueError("Unexpected --y-min value. Set numeric value or 'auto'.")
        if isinstance(args.y_max, str) and args.y_max != "auto":
            raise ValueError("Unexpected --y-max value. Set numeric value or 'auto'.")

        fig, ax = plt.subplots(
            figsize=(args.width / args.dpi, args.height / args.dpi),
            dpi=args.dpi,
        )
        fig.subplots_adjust(left=0.05, right=0.95, top=0.95, bottom=0.05)
        fig.canvas.manager.set_window_title("Plotter")
        ser = serial.Serial(args.port, args.rate)

        ax.set_xlim(0, args.x_ticks)
        if args.y_min != "auto":
            ax.set_ylim(bottom=args.y_min)
        if args.y_max != "auto":
            ax.set_ylim(top=args.y_max)
        ax.grid()

        x_data = []
        y_data = []
        line_plots = []

        buffer = b""
        dropped_frames = 0

        def animate(_):
            global buffer
            global dropped_frames

            buffer += ser.read_all()

            if b"\n" not in buffer:
                return line_plots
            *data, buffer = buffer.split(b"\n")

            for data_line in data:
                try:
                    data_line = data_line.decode()
                except UnicodeDecodeError:
                    print(f"[ERROR] UnicodeDecodeError: {data_line}")
                    return line_plots

                segments = []
                try:
                    for segment in data_line.strip().split():
                        segments.append(float(segment))
                except ValueError:
                    print(data_line)
                    return line_plots
                
                if not segments:
                    return line_plots
                
                if dropped_frames < args.drop_frames:
                    dropped_frames += 1
                    continue
                dropped_frames = 0
            

                for l, x, y, s in zip_longest(line_plots, x_data, y_data, segments):
                    if l is None:
                        x, y = [], []
                        x_data.append(x)
                        y_data.append(y)
                        (l,) = ax.plot(x, y, lw=1)
                        l.set_label(f"variable {len(line_plots)}")
                        ax.legend()

                        line_plots.append(l)

                    if len(x) < args.x_ticks:
                        x.append(len(x))
                    else:
                        y.pop(0)
                    y.append(s)


            for l, x, y in zip(line_plots, x_data, y_data):
                l.set_data(x, y)

            if args.y_min == "auto":
                ax.set_ylim(bottom=min(value for y in y_data for value in y if value is not None))
            if args.y_max == "auto":
                ax.set_ylim(top=max(value for y in y_data for value in y if value is not None))

            return lines

        ani = FuncAnimation(fig, animate, frames=1, interval=20, blit=False)
        plt.show()
