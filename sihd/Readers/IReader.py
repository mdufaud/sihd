#!/usr/bin/python
#coding: utf-8

""" System """
import time

from sihd import Core

class IReader(Core.IPolyService, Core.IAppContainer):

    def __init__(self, app=None, name="IReader"):
        super(IReader, self).__init__(name)
        self._set_default_conf({
            "service_type": "thread",
        })
        if (app):
            self.set_app(app)

    """ IProcessedService """

    def do_work(self) -> bool:
        return self.do_step()

    """ IService """

    def _pre_handle(self, channel) -> bool:
        if self.__can_save:
            if channel == self.save_data:
                self.set_saving_data(channel.read())
                return True
        return False

    """ IReader """

    def set_source(self, source) -> bool:
        raise NotImplementedError("set_source not implemented")

    def close(self) -> bool:
        return True

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)
