#!/usr/bin/python
# coding: utf-8

#
# System
#
import os
import sys
import logging

from sihd.gui.WxPython.AWxPythonGui import AWxPythonGui

try:
    import wx
    Panel = wx.Panel
    Frame = wx.Frame
except ImportError:
    wx = None
    Panel = object
    Frame = object

class WxPythonIpGui(AWxPythonGui):

    def __init__(self, name="WxPythonIpGui", app=None):
        global wx
        if wx is None:
            import wx
        super(WxPythonIpGui, self).__init__(app=app, name=name)
        self.configuration.add_defaults({
        })
        self.add_channel_input("message")

    def build_wx_frames(self, app):
        self.frame = MainIpWindow(None, title="Demo")
        self.frame.Show()

    #
    # Configuration
    #

    def on_setup(self, conf):
        return super().on_setup(conf)

    #
    # Channel
    #

    def handle(self, channel):
        message, co = channel.read()
        if self.frame and self.frame.logframe:
            message = message.decode().strip()
            if message != "":
                self.frame.logframe.log(message)
            if message == "stop":
                self.stop(True)
        return True

    def on_stop(self):
        super().on_stop()
        if self.frame:
            self.frame.Close()

class LogFrame(Panel):

    def __init__(self, *args, **kwargs):
        super(LogFrame, self).__init__(*args, **kwargs)
        self.createControls()
        self.bindEvents()
        self.doLayout()

    def createControls(self):
        self.logger = wx.TextCtrl(self,
                size=(800, 400),
                #style=wx.TE_MULTILINE | wx.TE_READONLY)
                style=wx.TE_MULTILINE)

    def bindEvents(self):
        self.logger.Bind(wx.EVT_TEXT, self.onTextEntered)

    def onTextEntered(self, event):
        text = event.GetString()

    def doLayout(self):
        boxSizer = wx.BoxSizer(orient=wx.HORIZONTAL)
        boxSizer.Add(self.logger)
        self.SetSizerAndFit(boxSizer)

    # Callback methods:

    def log(self, message):
        self.__log('Message: {}'.format(message))

    # Helper method(s):

    def __log(self, message):
        self.logger.AppendText('%s\n'%message)

class MainIpWindow(Frame):

    def __init__(self, *args, **kwargs):
        super(MainIpWindow, self).__init__(*args, **kwargs)
        self.CreateInteriorWindowComponents()
        self.CreateExteriorWindowComponents()

    def CreateInteriorWindowComponents(self):
        notebook = wx.Notebook(self)
        self.logframe = LogFrame(notebook)
        notebook.AddPage(self.logframe, 'Logger')
        self.SetClientSize(notebook.GetBestSize())

    def CreateExteriorWindowComponents(self):
        '''
            Create "exterior" window components, such as menu and status
            bar.
        '''
        self.CreateMenu()
        self.CreateStatusBar()
        self.SetTitle()

    def CreateMenu(self):
        fileMenu = wx.Menu()
        for id, label, helpText, handler in \
            [
                (wx.ID_ABOUT, '&About', 'Information about this program', self.OnAbout),
                (wx.ID_EXIT, 'E&xit', 'Terminate the program', self.OnExit)
            ]:
            if id == None:
                fileMenu.AppendSeparator()
            else:
                item = fileMenu.Append(id, label, helpText)
                self.Bind(wx.EVT_MENU, handler, item)

        menuBar = wx.MenuBar()
        menuBar.Append(fileMenu, '&File') # Add the fileMenu to the MenuBar
        self.SetMenuBar(menuBar)  # Add the menuBar to the Frame

    # Event handlers:

    def OnAbout(self, event):
        dialog = wx.MessageDialog(self, 'A WxPython Ip Gui',
                                    'About Sample Editor', wx.OK)
        dialog.ShowModal()
        dialog.Destroy()

    def OnExit(self, event):
        self.Close()  # Close the main window.

    def SetTitle(self):
        # MainWindow.SetTitle overrides wx.Frame.SetTitle, so we have to
        # call it using super:
        super(MainIpWindow, self).SetTitle('Ip GUI')
