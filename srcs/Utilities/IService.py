#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .ILoggable import ILoggable

class IService(ILoggable):

    def __init__(self, name="IService"):
        super(IService, self).__init__(name)
        self._stopped = True
        self._paused = False
        self._state_observers = set()

    def is_paused(self):
        return self._paused

    def is_running(self):
        return (not self._stopped)

    def is_active(self):
        return (not self._paused and not self._stopped)

    def get_state(self):
        s = "Service {}: ".format(self.get_name())
        if self._stopped:
            s += "Stopped"
        else:
            s += "Running"
        if self._paused:
            s += "(paused)"
        return s

    """ Start """

    def start(self, force=False):
        if self._stopped is False:
            self.log_debug("Starting an already started service")
            if force is False:
                return False
        if self._start_impl() is True:
            self._stopped = False
            self.__notify_change()
            self.log_debug("%s" %
                    ("is started" if not self._stopped else "did not start"))
        return (not self._stopped)

    def _start_impl(self):
        raise NotImplementedError("Start not implemented")

    """ Stop """

    def stop(self, force=False):
        if self._stopped is True:
            self.log_debug("Stopping an already stopped service")
            if force is False:
                return True
        if self._stop_impl() is True:
            self._stopped = True
            self.__notify_change()
            self.log_debug("%s" %
                    ("is stopped" if self._stopped else "did not stop"))
        return self._stopped

    def _stop_impl(self):
        raise NotImplementedError("Stop not implemented")

    """ Pause """

    def pause(self, force=False):
        if self._paused is True:
            self.log_debug("Pausing an already paused service")
            if force is False:
                return True
        if self._pause_impl() is True:
            self._paused = True
            self.__notify_change()
            self.log_debug("%s" %
                    ("is paused" if self._paused else "did not pause"))
        return self._paused

    def _pause_impl(self):
        self.log_debug("Called a non implemented function: pause")
        return False

    """ Resume """

    def resume(self, force=False):
        if self._paused is False:
            self.log_debug("Resuming a non paused service")
            if force is False:
                return True
        if self._pause_impl() is True:
            self._paused = False
            self.__notify_change()
            self.log_debug("%s" %
                    ("is resumed" if not self._paused else "did not resume"))
        return not self._paused

    def _resume_impl(self):
        self.log_debug("Called a non implemented function: resume")
        return False

    """ Reset """

    def reset(self):
        if self._reset_impl() is True:
            self._stopped = True
            self._paused = False
            self.__notify_change()
        return self._stopped and not self._paused

    def _reset_impl(self):
        self.log_debug("Called a non implemented function: reset")
        return False

    """ State Observation """

    def add_state_observer(self, obs):
        self._state_observers.add(obs)

    def __notify_change(self):
        for obs in self._state_observers:
            obs.service_state_changed(self, self._stopped, self._paused)

    """ Delete """

    def __service_exception(self, ex):
        import logging
        logger = logging.getLogger()
        logger.error("Service {} exception: {}".format(self.get_name(), ex))
        try:
            import traceback
            traceback.print_exc()
        except ImportError:
            pass

    def _del_impl(self):
        return

    def __del__(self):
        if self._stopped is False:
            try:
                self.stop()
            except Exception as ex:
                self.__service_exception(ex)
        try:
            self._del_impl()
        except Exception as ex:
            self.__service_exception(ex)

