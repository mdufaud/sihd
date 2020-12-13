#!/usr/bin/python
# coding: utf-8

import sihd
from sihd.core import RunnableThread
from sihd.core.SihdRunnableObject import SihdRunnableObject
from sihd.core.IAppContainer import IAppContainer

class AGui(SihdRunnableObject, IAppContainer):
    """
        Graphical user interface should be thread and not processes.
        If you want to be notified of channels input you have to call
        poll_channels_input() by yourself.
    """

    def __init__(self, name="AGui", app=None, parent=None, **kwargs):
        IAppContainer.__init__(self)
        if app and not parent:
            parent = app
        super().__init__(name, parent=parent, **kwargs)
        config = self.configuration
        config.add_defaults({
            "gui_side_thread": True,
        })
        config.load({
            "runnable_type": "thread",
            "runnable_frequency": 1,
        })
        config.set_dynamic('runnable_steps', 1)
        if app:
            self.set_app(app)
        self.__runnable = None

    #
    # SihdObject
    #

    def set_channel_notification(self, active):
        r = self.get_input_runnable()
        if not r:
            return
        if active:
            r.resume()
        else:
            r.pause()

    #
    # AGui
    #

    def get_input_runnable(self):
        return self.__runnable

    def _input_thread_step(self):
        self.poll_channels_input()

    def _input_thread_start(self, thread):
        pass

    def _input_thread_error(self, thread, iteration, error):
        self.log_error("{}: {}".format(thread, error))
        self.log_error(sihd.sys.get_traceback())

    def _input_thread_stop(self, thread, iteration):
        pass

    def start_gui_input_thread(self):
        config = self.configuration
        """ Set up a channel input polling thread """
        if self.__runnable is not None \
                and config.get("gui_input_thread") is True:
            return
        runnable = RunnableThread(
            daemon = True,
            name = "input_thread",
            parent = self,
            frequency = int(config.get("runnable_frequency")),
            timeout = 0,
            max_iter = 0,
            step = self._input_thread_step,
            on_start = self._input_thread_start,
            on_stop = self._input_thread_stop,
            on_err = self._input_thread_error)
        runnable.start()
        self.__runnable = runnable

    def on_runnable_start(self, thread, *args):
        if self.is_polling_channels_input:
            self.start_gui_input_thread()

    def on_stop(self):
        r = self.get_input_runnable()
        if not r:
            return
        r.stop()
        self.__runnable = None

    def on_resume(self):
        r = self.get_input_runnable()
        if not r:
            return
        r.resume()

    def on_pause(self):
        r = self.get_input_runnable()
        if not r:
            return
        r.pause()

    def on_step(self):
        self.loop()
        self.stop()

    #
    # To implement
    #

    def loop(self, *args, **kwargs):
        """
            To implement so it can be started without services
        """
        raise NotImplementedError("loop")

    #
    # IAppContainer
    #

    def set_app(self, app):
        super().set_app(app)
        app.add_gui(self)
