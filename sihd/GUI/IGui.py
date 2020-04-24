#!/usr/bin/python
#coding: utf-8

""" System """

from sihd import Core

class IGui(Core.IThreadedService, Core.IAppContainer):

    def __init__(self, app=None, name="IGui"):
        Core.IAppContainer.__init__(self)
        if app:
            self.set_app(app)
        super().__init__(name)
        self._set_default_conf({
            "service_type": "thread",
            "thread_frequency": 1,
            "thread_max_iterations": 1,
        })
        self.set_channel_notification(False)

    """ IGui """

    def loop(self, *args, **kwargs):
        raise NotImplementedError("No gui to loop onto")

    """ IAppContainer """

    def set_app(self, app):
        super().set_app(app)
        app.add_gui(self)
