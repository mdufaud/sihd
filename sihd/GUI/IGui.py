#!/usr/bin/python
#coding: utf-8

import sihd
from sihd import Core

class IGui(Core.IThreadedService, Core.IAppContainer):

    """
        Graphical user interface should be thread and not processes.
        If you want to be notified of channels input you have to call
        read_channels_input() by yourself.
    """

    def __init__(self, app=None, name="IGui"):
        self.__runnable = None
        Core.IAppContainer.__init__(self)
        super().__init__(name)
        self._set_default_conf({
            "service_type": "thread",
            "thread_frequency": 5,
            "thread_max_iterations": 1,
        })
        self.set_channel_notification(False)
        if app:
            self.set_app(app)

    """ IService """

    def set_channel_notification(self, active):
        r = self.get_input_runnable()
        if not r:
            return
        if active:
            r.resume_thread()
        else:
            r.pause_thread()

    """ IGui """

    def get_input_runnable(self):
        return self.__runnable

    def _thread_input_step(self):
        self.read_channels_input()

    def _thread_input_start(self, thread, *args):
        pass

    def _thread_input_error(self, thread, iteration, error):
        self.log_error("{}: {}".format(thread.name, error))
        self.log_error(sihd.get_traceback())

    def _thread_input_stop(self, thread, iteration):
        pass

    def on_thread_start(self, thread, *args):
        """ Set up a channel notification thread """
        runnable = Core.IRunnable()
        runnable.setup_thread(
            name="{}.InputThread".format(self.get_name()),
            frequency=int(self.get_conf("thread_frequency")),
            timeout=0,
            max_iter=0,
            step=self._thread_input_step,
            on_start=self._thread_input_start,
            on_stop=self._thread_input_stop,
            on_err=self._thread_input_error
        )
        runnable.start_thread()
        self.__runnable = runnable

    def on_stop(self):
        r = self.get_input_runnable()
        if not r:
            return
        r.stop_thread()

    def on_resume(self):
        r = self.get_input_runnable()
        if not r:
            return
        r.resume_thread()

    def on_pause(self):
        r = self.get_input_runnable()
        if not r:
            return
        r.pause_thread()

    def on_step(self):
        self.loop()
        self.stop()

    """ To implement """

    def loop(self, *args, **kwargs):
        """
            To implement so it can be started without services
        """
        raise NotImplementedError("No gui to loop onto")

    """ IAppContainer """

    def set_app(self, app):
        super().set_app(app)
        app.add_gui(self)
