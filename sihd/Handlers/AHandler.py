#!/usr/bin/python
#coding: utf-8

from sihd.Core.SihdRunnableService import SihdRunnableService
from sihd.Core.IAppContainer import IAppContainer

class AHandler(SihdRunnableService, IAppContainer):

    def __init__(self, name="AHandler", app=None, parent=None, **kwargs):
        IAppContainer.__init__(self)
        if app and not parent:
            parent = app
        super().__init__(name, parent=parent, **kwargs)
        if app:
            self.set_app(app)
        self.set_default_conf({
            "runnable_type": "thread",
        })

    #
    # AHandler
    #

    def handle_service(self, service) -> bool:
        cls = service.__class__.__name__.lower()
        try:
            method = getattr(self, "handle_service_{}".format(cls))
        except AttributeError:
            self.log_error("Cannot handle service {}".format(cls))
            return False
        method(service)
        return True

    #
    # SihdService
    #

    """
    def handle(self, channel):
        raise NotImplementedError("handle not implemented")
    """

    #
    # IAppContainer
    #

    def set_app(self, app):
        super().set_app(app)
        app.add_handler(self)
