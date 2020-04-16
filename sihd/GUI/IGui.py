#!/usr/bin/python
#coding: utf-8

""" System """

from sihd import Core

class IGui(Core.IPolyService, Core.IAppContainer):

    def __init__(self, app=None, name="IGui"):
        super(IGui, self).__init__(name)
        if (app):
            self.set_app(app)

    """ IGui """

    def gui_loop(self, *args, **kwargs):
        raise NotImplementedError("gui_loop not implemented")

    """ IAppContainer """

    def set_app(self, app):
        super(IGui, self).set_app(app)
        app.add_gui(self)
