#!/usr/bin/python
#coding: utf-8

""" System """
import sys
import logging

curses = None
locale = None

from sihd.srcs.GUI.IGui import IGui
from sihd.srcs import Core

class ICursesGui(IGui):

    def __init__(self, app=None, name="ICursesGui"):
        global locale
        if locale is None:
            import locale
        locale.setlocale(locale.LC_ALL, '')
        global curses
        if curses is None:
            import curses, curses.panel
        super(ICursesGui, self).__init__(app=app, name=name)
        self._log_handler = None
        self.__curses_on = False
        self.__windows = {}
        self.__panels = {}
        self.__movable = {}

    # Core

    def is_curses_on(self):
        return self.__curses_on

    def add_window(self, window, name, warning=True):
        win = self.get_window(name)
        if win is not None:
            if warning:
                self.log_warning("Window {} already exist".format(name))
        else:
            self.__windows[name] = window
        return win is None

    def add_panel(self, window, panel, name):
        self.add_window(window, name, warning=False)
        p = self.get_panel(name)
        if p is not None:
            if warning:
                self.log_warning("Panel {} already exist".format(name))
        else:
            self.__panels[name] = panel
        return p is None

    def get_panel(self, name):
        return self.__panels.get(name, None)

    def get_window(self, name):
        return self.__windows.get(name, None)

    def refresh_windows(self, full=False):
        if full:
            for _, win in self.__windows.items():
                self.move_x(win, 0)
                m = self.__movable[win]
                win.resize(m["h"], m["w"])
        for _, win in self.__windows.items():
            win.refresh()

    def clear_windows(self):
        for _, win in self.__windows.items():
            win.clear()

    def update_panels(self, full=False):
        if full:
            for _, panel in self.__panels.items():
                self.move_x(panel, 0)
        curses.panel.update_panels()
        self.stdscr.refresh()

    def __add_movable(self, movable, h, w, y, x):
        self.__movable[movable] = {
            "w": int(w),
            "h": int(h),
            "x": int(x),
            "y": int(y),
        }

    def move_y(self, movable, n):
        obj = self.__movable.get(movable, None)
        if obj is None:
            self.log_error("No curses item found to move")
            return False
        h = obj["h"]
        y = obj["y"]
        if y + n >= (curses.LINES - h) or y + n <= 0:
            return False
        move_fun = None
        try:
            move_fun = movable.mvwin
        except AttributeError:
            move_fun = movable.move
        ret = True
        try:
            move_fun(y + n, obj["x"])
            obj["y"] = y + n
        except (curses.error, curses.panel.error):
            ret = False
        return ret

    def move_x(self, movable, n):
        obj = self.__movable.get(movable, None)
        if obj is None:
            self.log_error("No curses item found to move")
            return False
        w = obj["w"]
        x = obj["x"]
        if (x + n) >= (curses.COLS - w) or x + n < 0:
            return False
        move_fun = None
        try:
            move_fun = movable.mvwin
        except AttributeError:
            move_fun = movable.move
        ret = True
        try:
            move_fun(obj["y"], x + n)
            obj["x"] = x + n
        except (curses.error, curses.panel.error):
            ret = False
        return ret

    # Setup

    def _set_win_log(self, win):
        # Create a logger in curses and removes stream logger
        win.scrollok(True)
        win.leaveok(True)
        Core.ILoggable.remove_stream_handlers()
        log_handler = CursesHandler(win)
        log_handler.setFormatter(Core.ILoggable.get_formatter())
        Core.ILoggable.logger.addHandler(log_handler)
        self._log_handler = log_handler

    def create_window(self, height, width, begin_y, begin_x):
        win = curses.newwin(int(height), int(width),
                            int(begin_y), int(begin_x))
        self.__add_movable(win, height, width, begin_y, begin_x)
        return win

    def create_panel(self, height, width, begin_y, begin_x):
        win = self.create_window(height, width, begin_y, begin_x)
        win.erase()
        panel = curses.panel.new_panel(win)
        self.__add_movable(panel, height, width, begin_y, begin_x)
        return win, panel

    def init_curses(self):
        if self.__curses_on is True:
            return
        self.log_info("Adding curses")
        stdscr = curses.initscr()
        self.stdscr = stdscr
        curses.noecho()
        curses.curs_set(0)
        curses.cbreak()
        self.stdscr.keypad(1)
        curses.start_color()
        curses.init_pair(1, curses.COLOR_RED, curses.COLOR_WHITE)
        self.__curses_on = True

    def remove_curses(self):
        if self.__curses_on is False:
            return
        curses.nocbreak()
        self.stdscr.keypad(0)
        curses.curs_set(1)
        curses.echo()
        curses.endwin()
        self.stdscr = None
        self.__curses_on = False
        self.log_info("Removed curses")

    def __default_windows(self):
        main_win = self.create_window((curses.LINES / 2) - 1,
                                    curses.COLS - 1,
                                    0, 0)
        self.add_window(main_win, "main")
        log_win = self.create_window((curses.LINES / 2) - 1,
                                    curses.COLS - 1,
                                    (curses.LINES / 2), 0)
        self.add_window(log_win, "log")
        self._set_win_log(log_win)

    # Children implementations

    def setup_windows(self):
        self.__default_windows()
        return False

    def resize(self):
        self.stdscr.clear()
        self.stdscr.refresh()
        
    def input(self, char, curses):
        return False

    # Entry point

    def gui_loop(self):
        self.init_curses()
        stdscr = self.stdscr
        stdscr.clear()
        stdscr.refresh()
        self.setup_windows()
        stdscr.nodelay(True)
        while self.is_curses_on():
            c = stdscr.getch()
            if c == curses.KEY_RESIZE:
                self.resize()
            elif c != -1 and self.input(c, curses) == False:
                break
        self.stop()

    # Services

    def _pause_impl(self):
        self.pause_thread()
        return True

    def _resume_impl(self):
        self.resume_thread()
        return True

    def _start_impl(self):
        self.setup_thread()
        self.start_thread()
        return True

    def _stop_impl(self):
        if self._log_handler is not None:
            Core.ILoggable.logger.removeHandler(self._log_handler)
            self._log_handler = None
            Core.ILoggable.add_stream_handler()
        self.remove_curses()
        self.stop_thread()
        return True

# Logging handler

try:
    unicode
    _unicode = True
except NameError:
    _unicode = False

class CursesHandler(logging.Handler):

    def __init__(self, screen):
        logging.Handler.__init__(self)
        self.screen = screen
        self.code = locale.getpreferredencoding()

    def emit(self, record):
        try:
            msg = self.format(record)
            screen = self.screen
            fs = "\n%s"
            to_print = None
            if not _unicode: #if no unicode support...
                to_print = fs % msg
            else:
                try:
                    if (isinstance(msg, unicode)):
                        ufs = u'\n%s'
                        try:
                            to_print = ufs % msg
                        except UnicodeEncodeError:
                            to_print = (ufs % msg).encode(self.code)
                    else:
                        to_print = fs % msg
                except UnicodeError:
                    to_print = fs % msg.encode("UTF-8")
            screen.addstr(to_print)
            screen.refresh()
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            self.handleError(record)
