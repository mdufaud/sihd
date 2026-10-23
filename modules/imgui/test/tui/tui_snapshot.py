import argparse
import sys

import pyte

from tui_terminal import TuiTerminal

NAMED_COLORS = {
    "black": "000000",
    "red": "cd0000",
    "green": "00cd00",
    "brown": "cdcd00",
    "blue": "0000ee",
    "magenta": "cd00cd",
    "cyan": "00cdcd",
    "white": "e5e5e5",
    "brightblack": "7f7f7f",
    "brightred": "ff0000",
    "brightgreen": "00ff00",
    "brightbrown": "ffff00",
    "brightblue": "5c5cff",
    "brightmagenta": "ff00ff",
    "brightcyan": "00ffff",
    "brightwhite": "ffffff",
}


def sgr_color(color: str, layer: int) -> str:
    hex_color = NAMED_COLORS.get(color, color)
    if len(hex_color) != 6:
        return f"\x1b[{layer + 1}m"
    red, green, blue = (int(hex_color[i : i + 2], 16) for i in (0, 2, 4))
    return f"\x1b[{layer};2;{red};{green};{blue}m"


def ansi_lines(screen: pyte.Screen) -> list[str]:
    lines: list[str] = []
    for y in range(screen.lines):
        row = screen.buffer[y]
        parts: list[str] = []
        for x in range(screen.columns):
            char = row[x]
            parts.append(sgr_color(char.fg, 38) + sgr_color(char.bg, 48) + (char.data or " "))
        lines.append("".join(parts) + "\x1b[0m")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description="Print the emulated terminal screen of the imgui ncurses scene")
    parser.add_argument("--size", default="100x30", help="COLSxROWS")
    parser.add_argument("--term", default="xterm-256color", help="TERM given to the scene")
    parser.add_argument("--locale", default="C.UTF-8", help="LC_ALL given to the scene")
    parser.add_argument("--wait", type=float, default=1.0, help="seconds to let the scene render")
    parser.add_argument("--ansi", action="store_true", help="reproduce colors with 24-bit escapes")
    args = parser.parse_args()

    cols, rows = (int(value) for value in args.size.split("x"))
    with TuiTerminal(cols=cols, rows=rows, term=args.term, locale=args.locale) as term:
        term.wait_for(lambda _lines: False, args.wait)
        output = ansi_lines(term.screen) if args.ansi else term.lines()
    sys.stdout.write("\n".join(output) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
