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
        self.__can_save = False
        if (app):
            self.set_app(app)

    """ IProcessedService """

    def do_work(self, i: int) -> bool:
        return self.do_step()

    """ IService """

    def _pre_handle(self, channel) -> bool:
        if self.__can_save:
            if channel == self.c_save:
                self.set_saving_data(channel.read())
                return True
        return False

    """ IDumpable """

    def _dump(self):
        dic = super()._dump()
        if self.__can_save:
            saved_data = self.get_data_saved()
            dic.update({"data_saved": self.__data_saved})
        return dic

    def _load(self, dic):
        super()._load(dic)
        if self.__can_save:
            data_saved = dic.get("data_saved", None)
            if data_saved is not None:
                self.c_saved.write(data_saved)

    """ Reader """

    def set_reader_saving(self, active=True, size=None, var_type=None):
        """ Init a reader capability to save data """
        if active:
            self.__saving_data = False
            self.__channel_to_save = None
            self.add_channel_input("c_save", type="bool", poll=True)
            self.add_channel_output("c_saved", type="list",
                                    size=size, var_type=var_type)
        self.__can_save = active

    def set_source(self, source):
        raise NotImplementedError("set_source not implemented")

    def set_channel_saving(self, channel: "str or Channel"):
        """ Chooses a channel to save data from"""
        if self.__can_save:
            self.__channel_to_save = channel

    def is_saving_data(self):
        return self.__can_save and self.__saving_data

    def set_saving_data(self, active: bool):
        """ Activate a reader capability to save data """
        if self.__can_save is False:
            return
        chan_save = self.__channel_to_save
        if chan_save is not None:
            if active:
                chan_save.add_observer(self.c_saved)
            else:
                chan_save.remove_observer(self.c_saved)
            self.__saving_data = active

    def get_data_saved(self):
        return [] if self.__can_save is False else self.c_saved.get_data()

    def clear_data_saved(self):
        if self.__can_save:
            self.c_saved.clear()

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)
