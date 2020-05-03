#!/usr/bin/python
#coding: utf-8

from sihd.Core.SihdRunnableService import SihdRunnableService
from sihd.Core.IAppContainer import IAppContainer

class AReader(SihdRunnableService, IAppContainer):

    def __init__(self, app=None, name="AReader", **kwargs):
        IAppContainer.__init__(self)
        super().__init__(name, **kwargs)
        if app:
            self.set_app(app)
        self._set_default_conf({
            "runnable_type": "thread",
        })

    """ AReader """

    def set_source(self, source) -> bool:
        raise NotImplementedError("set_source not implemented")

    def close(self) -> bool:
        return True

    """ SihdService """

    def on_stop(self):
        self.close()

    """ IAppContainer """

    def set_app(self, app):
        super().set_app(app)
        self.set_namedobject_parent(app)
        app.add_reader(self)
