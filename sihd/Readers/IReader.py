#!/usr/bin/python
#coding: utf-8

""" System """
import time

from sihd import Core

class IReader(Core.IPolyService, Core.IAppContainer):

    def __init__(self, app=None, name="IReader"):
        Core.IAppContainer.__init__(self)
        super().__init__(name)
        if app:
            self.set_app(app)
        self._set_default_conf({
            "service_type": "thread",
        })

    """ IProcessedService """

    def on_work(self) -> bool:
        return self.on_step()

    """ IReader """

    def set_source(self, source) -> bool:
        raise NotImplementedError("set_source not implemented")

    def close(self) -> bool:
        return True

    """ IAppContainer """

    def set_app(self, app):
        super().set_app(app)
        app.add_reader(self)
