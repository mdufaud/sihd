from collections.abc import Iterator

import pytest

from tui_terminal import TuiTerminal

KEY_DOWN = b"\x1bOB"
KEY_TAB = b"\t"
KEY_BACKSPACE = b"\x7f"
KEY_CTRL_A = b"\x01"
KEY_CTRL_C = b"\x03"
KEY_CTRL_H = b"\x08"
KEY_SHIFT_TAB = b"\x1b[Z"
MOUSE_CTRL = 16


def open_scene(cols: int = 100, rows: int = 30, term: str = "xterm-256color", locale: str = "C.UTF-8") -> TuiTerminal:
    terminal = TuiTerminal(cols=cols, rows=rows, term=term, locale=locale)
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


def test_any_motion_mouse_mode_is_enabled(term: TuiTerminal) -> None:
    assert b"\x1b[?1003h" in term.raw


def test_ctrl_c_exits_and_restores_terminal(term: TuiTerminal) -> None:
    term.send(KEY_CTRL_C)

    assert term.wait_exit(3.0)
    assert b"\x1b[?1003l" in term.raw
    assert b"\x1b[?1049l" in term.raw
