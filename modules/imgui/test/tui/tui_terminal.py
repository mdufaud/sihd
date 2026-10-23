import fcntl
import os
import pty
import re
import select
import signal
import struct
import termios
import time
from collections.abc import Callable
from pathlib import Path
from types import TracebackType
from typing import ClassVar, Self

import pyte

REPO_ROOT = Path(__file__).resolve().parents[4]
DEFAULT_SCENES_BIN = REPO_ROOT / "build" / "last" / "bin" / "sihd_imgui_ncurses_scenes"
SGR = re.compile(rb"\x1b\[([0-9;:]*)m")
UNFINISHED_ESCAPE = re.compile(rb"\x1b(\[[0-9;:?<>=]*)?$")


def semicolon_sgr(match: re.Match[bytes]) -> bytes:
    # pyte only reads 24-bit colours as 38;2;r;g;b, ncurses sends the ITU 38:2::r:g:b form for xterm-direct
    return b"\x1b[" + match.group(1).replace(b"::", b":").replace(b":", b";") + b"m"


class RepeatingScreen(pyte.Screen):
    # ncurses compresses runs of one character with REP (CSI n b), which pyte does not know
    def __init__(self, columns: int, lines: int) -> None:
        super().__init__(columns, lines)
        self._last_char = " "

    def draw(self, data: str) -> None:
        super().draw(data)
        if data:
            self._last_char = data[-1]

    def repeat_last_character(self, count: int = 1) -> None:
        self.draw(self._last_char * max(count, 1))

    # ncurses scrolls shifted rows with SU/SD (CSI n S, CSI n T), which pyte does not know either
    def scroll_up(self, count: int = 1) -> None:
        top, bottom = self.margins or pyte.screens.Margins(0, self.lines - 1)
        x, y = self.cursor.x, self.cursor.y
        self.cursor.y = bottom
        for _ in range(max(count, 1)):
            self.index()
        self.cursor.x, self.cursor.y = x, y

    def scroll_down(self, count: int = 1) -> None:
        top, bottom = self.margins or pyte.screens.Margins(0, self.lines - 1)
        x, y = self.cursor.x, self.cursor.y
        self.cursor.y = top
        for _ in range(max(count, 1)):
            self.reverse_index()
        self.cursor.x, self.cursor.y = x, y


class RepeatingStream(pyte.ByteStream):
    csi: ClassVar[dict[str, str]] = {
        **pyte.ByteStream.csi,
        "b": "repeat_last_character",
        "S": "scroll_up",
        "T": "scroll_down",
    }


class TuiTerminal:
    def __init__(self, cols: int = 100, rows: int = 30, term: str = "xterm-256color", locale: str = "C.UTF-8") -> None:
        binary = Path(os.environ.get("SIHD_IMGUI_SCENES_BIN", DEFAULT_SCENES_BIN))
        if not binary.is_file():
            raise FileNotFoundError(f"{binary} missing: build with 'make m=imgui test=1' or set SIHD_IMGUI_SCENES_BIN")
        env = dict(os.environ, TERM=term, LC_ALL=locale)
        self.screen = RepeatingScreen(cols, rows)
        self._stream = RepeatingStream(self.screen)
        self.raw = bytearray()
        self._unfinished = b""
        self._closed = False
        self._pid, self._fd = pty.fork()
        if self._pid == 0:
            try:
                fcntl.ioctl(pty.STDOUT_FILENO, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
                os.execve(binary, [str(binary)], env)
            finally:
                os._exit(127)
        self._pidfd = os.pidfd_open(self._pid)

    def __enter__(self) -> Self:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        self.close()

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        if not self.exited():
            os.kill(self._pid, signal.SIGKILL)
        os.waitpid(self._pid, 0)
        os.close(self._pidfd)
        os.close(self._fd)

    def exited(self) -> bool:
        ready, _, _ = select.select([self._pidfd], [], [], 0)
        return bool(ready)

    def pump(self, timeout: float) -> bool:
        ready, _, _ = select.select([self._fd], [], [], timeout)
        if not ready:
            return False
        try:
            data = os.read(self._fd, 65536)
        except OSError:
            return False
        self.raw += data
        pending = self._unfinished + data
        unfinished = UNFINISHED_ESCAPE.search(pending)
        cut = unfinished.start() if unfinished else len(pending)
        self._unfinished = pending[cut:]
        self._stream.feed(SGR.sub(semicolon_sgr, pending[:cut]))
        return bool(data)

    def lines(self) -> list[str]:
        return [line.rstrip() for line in self.screen.display]

    def wait_for(self, predicate: Callable[[list[str]], bool], timeout: float = 5.0) -> bool:
        deadline = time.monotonic() + timeout
        while not predicate(self.lines()):
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.pump(remaining):
                return predicate(self.lines())
        return True

    def line_starting_with(self, prefix: str) -> str:
        contents = (line.strip(" │|") for line in self.lines())
        return next((content for content in contents if content.startswith(prefix)), "")

    def wait_for_line(self, expected: str, timeout: float = 5.0) -> str:
        prefix = expected.split("=", 1)[0] + "="
        self.wait_for(lambda _lines: self.line_starting_with(prefix) == expected, timeout)
        return self.line_starting_with(prefix)

    def find(self, text: str) -> tuple[int, int] | None:
        for y, line in enumerate(self.lines()):
            x = line.find(text)
            if x != -1:
                return x, y
        return None

    def send(self, data: bytes) -> None:
        os.write(self._fd, data)

    def click(self, x: int, y: int, button: int = 0) -> None:
        self.send(f"\x1b[<{button};{x + 1};{y + 1}M\x1b[<{button};{x + 1};{y + 1}m".encode())

    def press(self, x: int, y: int) -> None:
        self.send(f"\x1b[<0;{x + 1};{y + 1}M".encode())

    def drag_to(self, x: int, y: int) -> None:
        self.send(f"\x1b[<32;{x + 1};{y + 1}M".encode())

    def release(self, x: int, y: int) -> None:
        self.send(f"\x1b[<0;{x + 1};{y + 1}m".encode())

    def wait_exit(self, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        watched = [self._fd, self._pidfd]
        while (remaining := deadline - time.monotonic()) > 0:
            ready, _, _ = select.select(watched, [], [], remaining)
            if self._pidfd in ready:
                while self.pump(0):
                    pass
                return True
            if not self.pump(0):
                watched = [self._pidfd]
        return False
