import re
import time
from collections.abc import Iterator

import pytest

from tui_terminal import TuiTerminal

KEY_DOWN = b"\x1bOB"
KEY_TAB = b"\t"
KEY_BACKSPACE = b"\x7f"
KEY_CTRL_A = b"\x01"
KEY_CTRL_C = b"\x03"
KEY_CTRL_H = b"\x08"
KEY_CTRL_LEFT = b"\x1b[1;5D"
KEY_CTRL_V = b"\x16"
KEY_CTRL_X = b"\x18"
KEY_SHIFT_TAB = b"\x1b[Z"
MOUSE_MIDDLE = 1
MOUSE_RIGHT = 2
MOUSE_CTRL = 16


def open_scene(
    cols: int = 100, rows: int = 30, term: str = "xterm-256color", locale: str = "C.UTF-8", colorterm: str | None = None
) -> TuiTerminal:
    terminal = TuiTerminal(cols=cols, rows=rows, term=term, locale=locale, colorterm=colorterm)
    if terminal.wait_for_line("ready=1") != "ready=1":
        terminal.close()
        pytest.fail("scene did not render:\n" + "\n".join(terminal.lines()))
    return terminal


@pytest.fixture
def term() -> Iterator[TuiTerminal]:
    with open_scene() as terminal:
        yield terminal


def wait_find(term: TuiTerminal, text: str) -> tuple[int, int] | None:
    term.wait_for(lambda _lines: term.find(text) is not None)
    return term.find(text)


def click_label(term: TuiTerminal, label: str, button: int = 0) -> None:
    position = wait_find(term, label)
    assert position is not None, "\n".join(term.lines())
    term.click(*position, button=button)


def settle(term: TuiTerminal, seconds: float) -> None:
    deadline = time.monotonic() + seconds
    while (remaining := deadline - time.monotonic()) > 0:
        term.pump(remaining)


def counter(term: TuiTerminal, name: str) -> int:
    return int(term.line_starting_with(f"{name}=").removeprefix(f"{name}="))


def frames_between_two_downs(term: TuiTerminal, seconds: float) -> int:
    # a frame can reach the screen in several reads: down_frame only changes on a press, wait for its row
    term.send(KEY_DOWN)
    term.wait_for_line("down=1 backspace=0 tab=0 backtab=0")
    settle(term, 0.2)
    first = counter(term, "down_frame")
    settle(term, seconds)
    term.send(KEY_DOWN)
    term.wait_for_line("down=2 backspace=0 tab=0 backtab=0")
    settle(term, 0.2)
    return counter(term, "down_frame") - first


def test_arrow_down_is_pressed(term: TuiTerminal) -> None:
    term.send(KEY_DOWN)

    assert term.wait_for_line("down=1 backspace=0 tab=0 backtab=0") == "down=1 backspace=0 tab=0 backtab=0"


def test_burst_of_arrows_counts_every_press(term: TuiTerminal) -> None:
    term.send(KEY_DOWN * 3)

    assert term.wait_for_line("down=3 backspace=0 tab=0 backtab=0") == "down=3 backspace=0 tab=0 backtab=0"


def test_typed_ascii_reaches_input(term: TuiTerminal) -> None:
    term.send(b"ab")

    assert term.wait_for_line("text=[ab]") == "text=[ab]"


def test_backspace_erases_last_character(term: TuiTerminal) -> None:
    term.send(b"ab")
    term.wait_for_line("text=[ab]")

    term.send(KEY_BACKSPACE)

    assert term.wait_for_line("text=[a]") == "text=[a]"


def test_ctrl_h_erases_last_character(term: TuiTerminal) -> None:
    term.send(b"ab")
    term.wait_for_line("text=[ab]")

    term.send(KEY_CTRL_H)

    assert term.wait_for_line("text=[a]") == "text=[a]"


def test_ctrl_a_then_typing_replaces_the_text(term: TuiTerminal) -> None:
    term.send(b"ab")
    term.wait_for_line("text=[ab]")

    term.send(KEY_CTRL_A + b"c")

    assert term.wait_for_line("text=[c]") == "text=[c]"


def test_typed_utf8_reaches_input(term: TuiTerminal) -> None:
    term.send("é".encode())

    assert term.wait_for_line("text=[é]") == "text=[é]"


def test_ctrl_left_moves_the_caret_one_word_back(term: TuiTerminal) -> None:
    term.send(b"ab cd")
    term.wait_for_line("text=[ab cd]")

    term.send(KEY_CTRL_LEFT + b"X")

    assert term.wait_for_line("text=[ab Xcd]") == "text=[ab Xcd]"


def test_alt_letter_is_a_shortcut_not_a_character(term: TuiTerminal) -> None:
    term.send(b"\x1bx")

    assert term.wait_for_line("alt_x=1") == "alt_x=1"
    assert term.line_starting_with("text=") == "text=[]"


def test_pasted_text_is_typed_without_its_newline_pressing_enter(term: TuiTerminal) -> None:
    assert b"\x1b[?2004h" in term.raw

    term.send(b"\x1b[200~hello\rworld\x1b[201~")
    term.wait_for_line("text=[helloworld]")
    term.send(b"!")

    assert term.wait_for_line("text=[helloworld!]") == "text=[helloworld!]"


def test_cut_text_goes_to_the_terminal_clipboard_and_pastes_back(term: TuiTerminal) -> None:
    term.send(b"ab")
    term.wait_for_line("text=[ab]")
    term.send(KEY_CTRL_A)
    settle(term, 0.1)

    term.send(KEY_CTRL_X)
    term.wait_for_line("text=[]")
    term.send(KEY_CTRL_V)
    term.wait_for_line("text=[ab]")
    term.send(KEY_CTRL_V)

    assert term.wait_for_line("text=[abab]") == "text=[abab]"
    assert b"\x1b]52;c;YWI=\x07" in term.raw


def test_unknown_escape_sequence_types_nothing(term: TuiTerminal) -> None:
    term.send(b"\x1b[99~a")

    assert term.wait_for_line("text=[a]") == "text=[a]"


def test_shift_tab_is_pressed(term: TuiTerminal) -> None:
    term.send(KEY_SHIFT_TAB)

    assert term.wait_for_line("down=0 backspace=0 tab=0 backtab=1") == "down=0 backspace=0 tab=0 backtab=1"


def test_click_toggles_checkbox(term: TuiTerminal) -> None:
    click_label(term, "Toggle")

    assert term.wait_for_line("checked=1 presses=0 far=0 ctrl=0") == "checked=1 presses=0 far=0 ctrl=0"


def test_click_presses_button(term: TuiTerminal) -> None:
    click_label(term, "Press")

    assert term.wait_for_line("checked=0 presses=1 far=0 ctrl=0") == "checked=0 presses=1 far=0 ctrl=0"


def test_ctrl_click_holds_ctrl(term: TuiTerminal) -> None:
    click_label(term, "Press", button=MOUSE_CTRL)

    assert term.wait_for_line("checked=0 presses=1 far=0 ctrl=1") == "checked=0 presses=1 far=0 ctrl=1"


def test_middle_click_reaches_imgui(term: TuiTerminal) -> None:
    term.click(10, 20, button=MOUSE_MIDDLE)

    assert term.wait_for_line("clicks middle=1 right=0") == "clicks middle=1 right=0"


def test_right_click_reaches_imgui(term: TuiTerminal) -> None:
    term.click(10, 20, button=MOUSE_RIGHT)

    assert term.wait_for_line("clicks middle=0 right=1") == "clicks middle=0 right=1"


def test_click_beyond_column_223_presses_button() -> None:
    with open_scene(cols=240) as term:
        position = term.find("Far")
        assert position is not None
        assert position[0] > 223

        term.click(*position)

        assert term.wait_for_line("checked=0 presses=0 far=1 ctrl=0") == "checked=0 presses=0 far=1 ctrl=0"


@pytest.mark.parametrize(("grab_dx", "grab_dy"), [(0, 0), (1, 1)])
def test_dragging_the_resize_grip_resizes_the_window(term: TuiTerminal, grab_dx: int, grab_dy: int) -> None:
    grip = wait_find(term, "◢")
    assert grip is not None
    x, y = grip[0] + grab_dx, grip[1] + grab_dy

    term.press(x, y)
    term.wait_for_line("grip=20x6 open=1 held=1")
    term.drag_to(x + 5, y + 2)
    term.wait_for_line("grip=25x8 open=1 held=1")
    term.release(x + 5, y + 2)

    assert term.wait_for_line("grip=25x8 open=1 held=0") == "grip=25x8 open=1 held=0"


def test_close_button_closes_the_window(term: TuiTerminal) -> None:
    click_label(term, "×")

    assert term.wait_for_line("grip=20x6 open=0 held=0") == "grip=20x6 open=0 held=0"


def test_terminal_cursor_follows_the_text_input_caret(term: TuiTerminal) -> None:
    term.send(b"ab")
    term.wait_for_line("text=[ab]")

    status = term.find("checked=")
    assert status is not None
    input_row = status[1] + 1
    text_col = term.lines()[input_row].find("ab")
    assert not term.screen.cursor.hidden
    assert (term.screen.cursor.x, term.screen.cursor.y) == (text_col + 2, input_row)


def test_keyboard_focus_is_shown_in_reverse_video(term: TuiTerminal) -> None:
    term.send(KEY_TAB)
    term.wait_for_line("down=0 backspace=0 tab=1 backtab=0")

    position = term.find("Toggle")
    assert position is not None
    assert term.wait_for(lambda _lines: term.screen.buffer[position[1]][position[0]].reverse)


def test_window_frame_fills_the_whole_screen(term: TuiTerminal) -> None:
    lines = term.lines()

    assert lines[0] == "┌" + "─" * 98 + "┐"
    assert lines[-1] == "└" + "─" * 98 + "┘"


def test_button_background_differs_from_window(term: TuiTerminal) -> None:
    position = term.find("Press")
    assert position is not None
    x, y = position

    assert term.screen.buffer[y][x].bg != term.screen.buffer[y][x + 20].bg


@pytest.mark.parametrize(
    ("term_name", "locale", "top_left"),
    [
        ("xterm-256color", "C.UTF-8", "┌"),
        ("xterm-256color", "C", "+"),
        ("linux", "C.UTF-8", "┌"),
        ("xterm-direct", "C.UTF-8", "┌"),
        ("vt100", "C.UTF-8", "┌"),
    ],
)
def test_terminal_types_render_the_frame(term_name: str, locale: str, top_left: str) -> None:
    with open_scene(term=term_name, locale=locale) as term:
        assert term.line_starting_with("down=") == "down=0 backspace=0 tab=0 backtab=0"
        assert term.lines()[0][0] == top_left


def test_eight_colour_terminal_keeps_the_button_background() -> None:
    with open_scene(term="linux") as term:
        position = term.find("Press")
        assert position is not None
        x, y = position

        assert term.screen.buffer[y][x].bg != term.screen.buffer[y][x + 20].bg


def test_direct_colour_terminal_gets_24_bit_colours() -> None:
    with open_scene(term="xterm-direct") as term:
        position = term.find("Press")
        assert position is not None
        x, y = position

        assert b"\x1b[48:2::48:82:122m" in term.raw
        assert term.screen.buffer[y][x].bg == "30527a"


@pytest.mark.parametrize("colorterm", ["truecolor", "24bit"])
def test_colorterm_upgrades_a_256_colour_terminal_to_24_bit(colorterm: str) -> None:
    with open_scene(colorterm=colorterm) as term:
        position = term.find("Press")
        assert position is not None
        x, y = position

        assert b"\x1b[48:2::48:82:122m" in term.raw
        assert term.screen.buffer[y][x].bg == "30527a"


def test_colorterm_without_a_direct_terminfo_keeps_256_colours() -> None:
    with open_scene(term="screen-256color", colorterm="truecolor") as term:
        assert term.line_starting_with("down=") == "down=0 backspace=0 tab=0 backtab=0"
        assert b"\x1b[48:2:" not in term.raw


def test_frames_are_drawn_as_synchronized_updates(term: TuiTerminal) -> None:
    markers = re.findall(rb"\x1b\[\?2026([hl])", bytes(term.raw))

    assert len(markers) > 0
    assert markers == [b"h", b"l"] * (len(markers) // 2)


def test_any_motion_mouse_mode_is_enabled(term: TuiTerminal) -> None:
    assert b"\x1b[?1003h" in term.raw


def test_still_screen_drops_to_the_idle_frame_rate(term: TuiTerminal) -> None:
    settle(term, 1.5)

    assert frames_between_two_downs(term, 4.0) <= 130


def test_input_brings_back_the_full_frame_rate(term: TuiTerminal) -> None:
    settle(term, 1.5)

    assert frames_between_two_downs(term, 0.5) >= 15


def test_animation_keeps_the_full_frame_rate(term: TuiTerminal) -> None:
    click_label(term, "Animate")
    settle(term, 3.0)
    click_label(term, "Animate")
    settle(term, 0.3)

    assert counter(term, "tick") >= 130


def test_ctrl_c_exits_and_restores_terminal(term: TuiTerminal) -> None:
    term.send(KEY_CTRL_C)

    assert term.wait_exit(3.0)
    assert b"\x1b[?1003l" in term.raw
    assert b"\x1b[?2004l" in term.raw
    assert b"\x1b[?1049l" in term.raw
