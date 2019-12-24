#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import sys
import logging

curses = None

from ..IGui import IGui
from ... import Utilities

class ICursesGui(IGui):

    def __init__(self, app=None, name="ICursesGui"):
        global curses
        if curses is None:
            import curses
        super(ICursesGui, self).__init__(app=app, name=name)
        self.set_run_method(self.__start_gui)
        self._log_handler = None
        self.__curses_on = False

    # Utilities

    def _get_maxes(self):
        return self._stdscr.getmaxyx()

    # Setup

    def _set_log_win(self, win):
        # Create a logger in curses and removes stream logger
        Utilities.ILoggable.remove_stream_handlers()
        log_handler = CursesHandler(win)
        log_handler.setFormatter(Utilities.ILoggable.get_formatter())
        Utilities.ILoggable.logger.addHandler(log_handler)
        self._log_handler = log_handler

    def _create_win(self, height, width, y, x):
        win = curses.newwin(height, width, y, x)
        win.scrollok(True)
        win.idlok(True)
        win.leaveok(True)
        return win

    def init_curses(self):
        if self.__curses_on is True:
            return
        self.log_info("Adding curses")
        self._stdscr = curses.initscr()
        curses.noecho()
        curses.cbreak()
        self._stdscr.keypad(1)
        curses.start_color()
        curses.init_pair(1, curses.COLOR_RED, curses.COLOR_WHITE)
        self.__curses_on = True

    def remove_curses(self):
        if self.__curses_on is False:
            return
        curses.nocbreak()
        self._stdscr.keypad(0)
        curses.echo()
        curses.endwin()
        self.__curses_on = False
        self.log_info("Removed curses")

    # Children implementations

    def _create_windows(self):
        return False

    def input(self, char):
        return False

    # Entry point

    def __start_gui(self):
        self.init_curses()
        self._create_windows()
        stdscr = self._stdscr
        while 1:
            c = stdscr.getch()
            if self.input(c) == False:
                break
        self.get_app().stop()

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
            Utilities.ILoggable.logger.removeHandler(self._log_handler)
            self._log_handler = None
            Utilities.ILoggable.add_stream_handler()
        self.remove_curses()
        self.stop_thread()
        return True

try:
    unicode
    _unicode = True
except NameError:
    _unicode = False

class CursesHandler(logging.Handler):

    def __init__(self, screen):
        logging.Handler.__init__(self)
        self.screen = screen

    def emit(self, record):
        try:
            msg = self.format(record)
            screen = self.screen
            fs = "\n%s"
            if not _unicode: #if no unicode support...
                screen.addstr(fs % msg)
                screen.refresh()
            else:
                try:
                    if (isinstance(msg, unicode) ):
                        ufs = u'\n%s'
                        try:
                            screen.addstr(ufs % msg)
                            screen.refresh()
                        except UnicodeEncodeError:
                            screen.addstr((ufs % msg).encode(code))
                            screen.refresh()
                    else:
                        screen.addstr(fs % msg)
                        screen.refresh()
                except UnicodeError:
                    screen.addstr(fs % msg.encode("UTF-8"))
                    screen.refresh()
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            self.handleError(record)
