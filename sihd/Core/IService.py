#!/usr/bin/python
#coding: utf-8

from .ILoggable import ILoggable
from .IConfigurable import IConfigurable

class IService(ILoggable, IConfigurable):

    def __init__(self, name="IService"):
        super(IService, self).__init__(name)
        self.__stopped = True
        self.__paused = False
        self._state_observers = set()

    def is_paused(self):
        return self.__paused

    def is_running(self):
        return (not self.__stopped)

    def is_active(self):
        return (not self.__paused and not self.__stopped)

    def get_service_state(self):
        s = ""
        if self.__stopped:
            s += "Stopped"
        else:
            s += "Running"
        if self.__paused:
            s += " (paused)"
        return s

    """ Start """

    def start(self, force=False):
        if self.__stopped is False:
            self.log_debug("Starting an already started service")
            if force is False:
                return False
        if self._start_impl() is True:
            self.__stopped = False
            self.__notify_change()
        self.log_debug("%s" %
                ("is started" if not self.__stopped else "did not start"))
        return self.is_running()

    def _start_impl(self):
        return True

    """ Stop """

    def stop(self, force=False):
        if self.__stopped is True:
            self.log_debug("Stopping an already stopped service")
            if force is False:
                return True
        if self._stop_impl() is True:
            self.__stopped = True
            self.__notify_change()
        self.log_debug("%s" %
                ("is stopped" if self.__stopped else "did not stop"))
        return self.is_running() == False

    def _stop_impl(self):
        return True

    """ Pause """

    def pause(self, force=False):
        if self.__paused is True:
            self.log_debug("Pausing an already paused service")
            if force is False:
                return True
        if self._pause_impl() is True:
            self.__paused = True
            self.__notify_change()
        self.log_debug("%s" %
            ("is paused" if self.__paused else "did not pause"))
        return self.is_paused()

    def _pause_impl(self):
        return True

    """ Resume """

    def resume(self, force=False):
        if self.__paused is False:
            self.log_debug("Resuming a non paused service")
            if force is False:
                return True
        if self._resume_impl() is True:
            self.__paused = False
            self.__notify_change()
        self.log_debug("%s" %
                ("is resumed" if not self.__paused else "did not resume"))
        return self.is_paused() == False

    def _resume_impl(self):
        return True

    """ Reset """

    def reset(self):
        if self._reset_impl() is True:
            self.__stopped = True
            self.__paused = False
            self.__notify_change()
        return self.__stopped and not self.__paused

    def _reset_impl(self):
        self.log_debug("Called a non implemented function: reset")
        return False

    """ State Observation """

    def add_state_observer(self, obs):
        self._state_observers.add(obs)

    def __notify_change(self):
        for obs in self._state_observers:
            obs.service_state_changed(self, self.__stopped, self.__paused)
