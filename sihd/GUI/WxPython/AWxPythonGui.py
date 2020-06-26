#!/usr/bin/python
# coding: utf-8

#
# System
#
import os
import os.path
import sys
import logging

from sihd.GUI.AGui import AGui
from sihd import Core

wx = None

class AWxPythonGui(AGui):

    def __init__(self, name="AWxPythonGui", *args, **kwargs):
        global wx
        if wx is None:
            import wx
        self.__wx_app = None
        super().__init__(name=name, *args, **kwargs)

    def loop(self, **kwargs):
        app = wx.App(False)
        self.__wx_app = app
        self.build_wx_frames(app)
        app.MainLoop()

    def build_wx_frames(self, app):
        raise NotImplemented
