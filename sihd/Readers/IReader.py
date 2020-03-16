#!/usr/bin/python
#coding: utf-8

""" System """
from sihd import Core
import time

class IReader(Core.IPolyService, Core.IAppContainer, Core.IObserver):

    def __init__(self, app=None, name="IReader"):
        super(IReader, self).__init__(name)
        self._set_default_conf({
            "service_type": "thread",
        })
        self.__saving_data = False
        self.__data_saved = []
        if (app):
            self.set_app(app)

    """ IProcessedService """

    def do_work(self, i):
        return self.step_method()

    """ IDumpable """

    def _dump(self):
        dic = super()._dump()
        dic.update({"data_saved": self.__data_saved})
        return dic

    def _load(self, dic):
        super()._load(dic)
        data_saved = dic.get("data_saved", None)
        if data_saved:
            self.__data_saved.extend(data_saved)

    """ Reader """

    def set_source(self, source):
        raise NotImplementedError("set_source not implemented")

    def is_saving_data(self):
        return self.__saving_data

    def save_data(self, active):
        self.__saving_data = active
        #TODO
        """
        if active:
            self.deliver = self.__save_decorator(self.deliver)
        else:
            self.deliver = self.__default_deliver
        """

    def get_data_saved(self):
        return self.__data_saved

    def clear_data_saved(self):
        self.__data_saved = []

    def __save_decorator(self, fun):
        def wrapper(*args):
            self.__data_saved.append((time.time(), args))
            return fun(*args)
        return wrapper

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)
