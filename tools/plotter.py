import argparse
from itertools import zip_longest
from textwrap import dedent

import matplotlib.pyplot as plt
import serial
import serial.tools.list_ports
from matplotlib.animation import FuncAnimation

if __name__ == "__main__":
    description = dedent(
        """\
        Serial plotter, similar to the one in Arduino IDE.

        Example usage:
        > python plotter.py list
        > python plotter.py plot --port /dev/ttyUSB0 --rate 115200 --y-max 4096
        """
    )

    parser = argparse.ArgumentParser(description=description)
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
        type=float,
        default=0.0,
        help="Lower y-axis limit.",
    )
    plot_parser.add_argument(
        "--y-max",
        dest="y_max",
        type=float,
        default=1.0,
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
        help="How many frames/lines to drop when reading from serial port.",
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

        fig, ax = plt.subplots(
            figsize=(args.width / args.dpi, args.height / args.dpi),
            dpi=args.dpi,
        )
        fig.canvas.manager.set_window_title("Plotter")
        fig.tight_layout(pad=1)
        ser = serial.Serial(args.port, args.rate)

        ax.set_xlim(0, args.x_ticks)
        ax.set_ylim(args.y_min, args.y_max)
        ax.grid()

        x_data = []
        y_data = []
        lines = []

        def animate(_):
            try:
                for _ in range(args.drop_frames):
                    ser.readline()
                reading = ser.readline()
                reading = reading.decode().strip().split()
            except UnicodeDecodeError:
                print(" ".join(reading))
                return lines

            segments = []
            try:
                for segment in reading:
                    segments.append(float(segment))
            except ValueError:
                print(" ".join(reading))
                return lines

            for l, x, y, s in zip_longest(lines, x_data, y_data, segments):
                if l is None:
                    x, y = [], []
                    x_data.append(x)
                    y_data.append(y)
                    (l,) = ax.plot(x, y, lw=1)
                    l.set_label(f"variable {len(lines)}")
                    ax.legend()

                    lines.append(l)

                if len(x) < args.x_ticks:
                    x.append(len(x))
                else:
                    y.pop(0)
                y.append(s)

                l.set_data(x, y)

            return lines

        ani = FuncAnimation(fig, animate, frames=1, interval=20, blit=True)
        plt.show()
