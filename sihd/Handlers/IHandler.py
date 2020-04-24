#!/usr/bin/python
#coding: utf-8

""" System """

from sihd import Core

class IHandler(Core.IPolyService, Core.IAppContainer):

    def __init__(self, app=None, name="IHandler"):
        Core.IAppContainer.__init__(self)
        super().__init__(name)
        if app:
            self.set_app(app)

    """ IHandler """

    def handle_service(self, service) -> bool:
        return False

    """ IProcessedService """

    def on_work(self) -> bool:
        return self.on_step()

    """ IService """

    def handle(self, channel):
        raise NotImplementedError("handle not implemented")

    """ IAppContainer """

    def set_app(self, app):
        super().set_app(app)
        app.add_handler(self)
