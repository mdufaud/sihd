#!/usr/bin/python
# coding: utf-8

#
# System
#
import os
import os.path
import sys
import logging

from sihd.gui.AGui import AGui
from sihd import core

wx = None

class AWxPythonGui(AGui):

    def __init__(self, name="AWxPythonGui", *args, **kwargs):
        global wx
        if wx is None:
            import wx
        self.__wx_app = None
        super().__init__(name=name, *args, **kwargs)

    #override
    def on_init(self):
        ret = super().on_init()
        app = wx.App(False)
        self.__wx_app = app
        self.build_wx_frames(app)
        return ret

    #override
    def loop(self, **kwargs):
        app = self.__wx_app
        app.MainLoop()

    def build_wx_frames(self, app):
        raise NotImplementedError("build_wx_frames")
