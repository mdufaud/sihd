#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import unittest
import random
import utils
import sihd
logger = sihd.log.setup()

from sihd.handlers.AHandler import AHandler
from sihd.readers.AReader import AReader
try:
    import wx
    from sihd.gui.wxpython.AWxPythonGui import AWxPythonGui
    from sihd.gui.wxpython.utils import MainWindow, LogFrame

    class MainWindow(MainWindow.MainWindow):

        def __init__(self, *a, **kw):
            super().__init__(*a, **kw)
            self.build_frames()

        def build_interior_frame(self):
            notebook = wx.Notebook(self)
            self.logframe = LogFrame.LogFrame(notebook)
            self.logframe.add_handler()
            notebook.AddPage(self.logframe, 'Logger')
            self.SetClientSize(notebook.GetBestSize())

        def build_exterior_frame(self):
            super().build_exterior_frame()

        def on_exit(self, event):
            self.logframe.remove_handler()
            super().on_exit(event)

    class WxPythonGui(AWxPythonGui):

        def __init__(self, name="WxPythonGui", app=None):
            global wx
            if wx is None:
                import wx
            super().__init__(app=app, name=name)
            self.configuration.add_defaults({})

        def build_wx_frames(self, app):
            self.main_window = MainWindow(None, title="Demo")
            self.main_window.Show()

        def handle(self, channel):
            self.log_info("Channel {} -> {}".format(channel, channel.read()))

        def on_stop(self):
            if self.main_window:
                self.main_window.Close()
            super().on_stop()

except ImportError:
    pass

class TestReader(AReader):

    def __init__(self, app=None, name="TestReader"):
        super().__init__(app=app, name=name)
        self.add_channel('output1')
        self.add_channel('output2')
        self.add_channel('output3')
        self.add_channel('output4')
        self.add_channel('output5')
        self.configuration.set('runnable_frequency', 50)

    def on_step(self):
        self.output1.write(random.random())
        self.output2.write(random.random())
        self.output3.write(random.random())
        self.output4.write(random.random())
        self.output5.write(random.random())
        return True

class TestWxGui(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_gui(self):
        sihd.strings.set('wx.about_hdr', 'About test GUI')
        sihd.strings.set('wx.about_txt', 'Testing GUI')
        reader = TestReader()
        gui = WxPythonGui()
        gui.configuration.set('channels_input', 'observe')
        self.assertTrue(reader.init())
        reader.output1.add_observer(gui)
        reader.output2.add_observer(gui)
        reader.output3.add_observer(gui)
        reader.output4.add_observer(gui)
        reader.output5.add_observer(gui)
        self.assertTrue(gui.start())
        self.assertTrue(reader.start())
        max_secs = 10
        try:
            while gui.is_running():
                time.sleep(1)
                max_secs -= 1
                if max_secs <= 0:
                    break
        except Exception as e:
            logger.error("{}".format(e))
        self.assertTrue(gui.stop())
        self.assertTrue(reader.stop())
        time.sleep(0.5)

if __name__ == '__main__':
    unittest.main(verbosity=2)
