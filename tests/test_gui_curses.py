#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time

import test_utils
import sihd

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

import unittest

from sihd.GUI.Curses.ICursesGui import ICursesGui

class SimpleCurses(ICursesGui):

    def __init__(self, app=None, name="SimpleCurses"):
        super(SimpleCurses, self).__init__(app=app, name=name)

    def setup_windows(self):
        super().setup_windows()
        self.main = self.get_window("main")
        self.main.box()
        self.log_info("Curses rolling")
        self.__set_str("Hello")

        win1, panel1 = self.create_panel(10, 12, 5, 5)
        win1.addstr(2, 2, "Panel1")
        win1.box()
        self.add_panel(win1, panel1, "Panel1")

        win2, panel2 = self.create_panel(10, 12, 8, 8)
        win2.addstr(2, 2, "Panel2")
        win2.box()
        self.add_panel(win2, panel2, "Panel2")
        panel1.top()

        self.panel1 = panel1
        self.panel2 = panel2
        self.refresh_windows()
        self.update_panels(True)

    def __set_str(self, s):
        win = self.main
        win.clear()
        win.box()
        win.addstr(2, 3, "Main window: ")
        win.addstr(3, 3, "Press l to log and keypad to move panel2")
        win.addstr(4, 3, s)

    def input(self, key, curses):
        char = curses.keyname(key).decode()
        self.__set_str("Pressed: {}".format(char))
        if char == 'q':
            return False
        elif char == 'l':
            self.log_info("Logging !")
        if key == curses.KEY_UP:
            self.move_y(self.panel2, -1)
        elif key == curses.KEY_DOWN:
            self.move_y(self.panel2, 1)
        elif key == curses.KEY_LEFT:
            self.move_x(self.panel2, -1)
        elif key == curses.KEY_RIGHT:
            self.move_x(self.panel2, 1)
        self.refresh_windows()
        self.update_panels(True)
        return True

    def resize(self):
        super().resize()
        self.clear_windows()
        self.main.box()
        self.__set_str("-- Resized --")
        self.log_info("Resized")
        win1 = self.get_window("Panel1")
        win2 = self.get_window("Panel2")
        win1.addstr(2, 2, "Panel1")
        win1.box()
        win2.addstr(2, 2, "Panel2")
        win2.box()
        self.refresh_windows(True)
        self.update_panels(True)

class TestGui(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_simple_curses_gui(self):
        gui = SimpleCurses()
        self.assertTrue(gui.setup())
        self.assertTrue(gui.start())
        try:
            time.sleep(200)
        except KeyboardInterrupt:
            pass
        self.assertTrue(gui.stop())

if __name__ == '__main__':
    unittest.main(verbosity=2)
