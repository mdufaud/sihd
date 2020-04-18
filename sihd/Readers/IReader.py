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
            if channel == self.save_data:
                self.set_saving_data(channel.read())
                return True
        return False

    """ Reader """

    def set_source(self, source):
        raise NotImplementedError("set_source not implemented")

    def close(self):
        return True

    """
    def set_reader_saving(self, active=True, size=None, var_type=None):
        if active:
            if self.is_configured():
                raise RuntimeError("Must configure reader saving capability before setup")
            self.__channel_to_save = None
            self.add_channel_input("save_data", type="bool",
                                    block=True, timeout=0.5,
                                    default=False, poll=True)
            self.add_channel_output("saved_data", type="list",
                                    size=size, var_type=var_type)
        self.__can_save = active


    def set_channel_saving(self, channel: "str or Channel"):
        if self.__can_save:
            self.__channel_to_save = channel

    def is_saving_data(self):
        return self.__can_save and self.save_data.read()

    def set_saving_data(self, active: bool):
        if self.__can_save is False:
            return
        chan_save = self.__channel_to_save
        if isinstance(chan_save, str):
            chan_save = self.get_channel(chan_save)
        if chan_save is not None:
            self.log_error("SAVING {}".format(chan_save))
            if active:
                chan_save.add_observer(self.saved_data)
            else:
                chan_save.remove_observer(self.saved_data)
            return True
        else:
            self.log_error("Cannot find channel to save data")
        return False

    def get_data_saved(self):
        return [] if self.__can_save is False else self.saved_data.get_data()

    def clear_data_saved(self):
        if self.__can_save:
            self.saved_data.write([])
        return self.__can_save
    """

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)
