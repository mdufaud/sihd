#!/usr/bin/python
#coding: utf-8

""" System """

from sihd import Core

class IHandler(Core.IPolyService, Core.IAppContainer):

    def __init__(self, app=None, name="IHandler"):
        super(IHandler, self).__init__(name)
        if (app):
            self.set_app(app)

    def handle_service(self, service) -> bool:
        return False

    """ IProcessedService """

    def do_work(self, i: int) -> bool:
        return self.do_step()

    """ IService """

    def handle(self, channel):
        raise NotImplementedError("handle not implemented")

    """ IAppContainer """

    def set_app(self, app):
        super(IHandler, self).set_app(app)
        app.add_handler(self)
