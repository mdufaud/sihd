#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import sys
import time

try:
    import ConfigParser
except ImportError:
    import configparser
    ConfigParser = configparser

from .. import Utilities
from .. import Readers
from .. import Handlers
from .. import GUI
from .. import Interactors

class IApp(Utilities.IService, Utilities.ILoggable,
            Utilities.IConfigurable, Utilities.IDumpable,
            Utilities.IServiceStateObserver):

    def __init__(self, name="IApp", *args, **kwargs):
        super(IApp, self).__init__(name, *args, **kwargs)
        self._path = None
        self.readers = set()
        self.handlers = set()
        self.guis = set()
        self.interactors = set()
        self._conf_path = None
        self._path = IApp.get_actual_path()
        self._children_configured = False
        self.args = None
        self.__args_setted = None
        self._loop_impl = self._infinite_loop

    @staticmethod
    def get_actual_path():
        path = os.getcwd()
        return path

    def setup(self, *args, **kwargs):
        ret = False
        self.log_debug("Starting application setup")
        ret = self._setup_impl(*args, **kwargs)
        if ret is False:
            self.log_error("Application setup has failed")
        else:
            self.log_debug("Application successfully setup")
        return ret

    def _setup_impl(self):
        """ To be implemented by children """
        return False

    """
    ###############
    ARGS MANAGEMENT
    ###############
    """

    def define_args(self):
        """ To be implemented by children """
        return None

    def is_args(self):
        if len(sys.argv) == 1:
            return False
        return True

    def get_arg(self, name):
        args = self.args
        if args and args.contains(name) is True:
            return args.__getattribute__(name)
        return self.get_conf_val(name)

    def set_args(self, args):
        """ args = ['--foo', 'BAR'] - Bypass command line args """
        self.__args_setted = args

    def parse_args(self):
        parser = self.define_args();
        if parser is None:
            return None
        if self.__args_setted is not None:
            self.args = parser.parse_args(args=self.__args_setted)
        else:
            self.args = parser.parse_args()
        return self.args

    """
    ###############
    Configurations
    ###############
    """

    def load_app_conf(self, path=None):
        """ Load conf from path or <APP_PATH>/config/<APP_NAME>.ini """
        if path is None:
            filename = self.get_name() + ".ini"
            conf = os.path.join("config", filename)
            to_read = os.path.join(self._path, conf)
            self.__read_conf(to_read)
        else:
            self.__read_conf(path)

    def __setup_logger_conf(self, config):
        if self._path is None:
            self.error("No path set for app")
            return
        config.add_section("Logger")
        config.set("Logger", "level", "info")
        config.set("Logger", "directory", os.path.join(self._path, "logs"))

    def __read_conf(self, path):
        self._conf_path = path
        obj = ConfigParser.SafeConfigParser()
        if not os.path.isfile(path):
            self.say("Making conf file {}".format(self.get_conf_path()))
            self.__setup_logger_conf(obj)
        elif not obj.has_section("Logger"):
            self.__setup_logger_conf(obj)
        else:
            obj.read(path)
        self.load_conf(obj)
        Utilities.ILoggable.setup_log(self.get_name(),
                                    obj.get("Logger", "level"),
                                    obj.get("Logger", "directory"))
        self.log_debug("Logger is setup")

    def get_conf_path(self):
        return self._conf_path

    def _write_conf(self, obj=None):
        if obj is None:
            obj = self.get_conf_obj()
        with open(self._conf_path, 'w') as configfile:
            obj.write(configfile)
            self.log_debug("Conf file {} is written".format(self.get_conf_path()))

    def load_children_conf(self):
        conf = self.get_conf_obj()
        if conf is None:
            return
        if self._children_configured:
            self.log_debug("Services already configured")
            return False
        self._children_configured = True
        self.__call_children(self.interactors, "load_conf", conf)
        self.__call_children(self.guis, "load_conf", conf)
        self.__call_children(self.handlers, "load_conf", conf)
        self.__call_children(self.readers, "load_conf", conf)
        self.log_info("Services are configured")
        return True

    """
    ###############
    Utilities
    ###############
    """

    def set_path(self, path):
        self._path = path

    def get_path(self):
        return self._path

    def get_child(self, name):
        for reader in self.readers:
            if reader.get_name() == name:
                return reader
        for handler in self.handlers:
            if handler.get_name() == name:
                return handler
        for gui in self.guis:
            if gui.get_name() == name:
                return gui
        for interactor in self.interactors:
            if interactor.get_name() == name:
                return interactor
        self.log_error("children {0} not found".format(name))
        return None

    def __call_child(self, child, fun, arg=None):
        ret = False
        if arg is None:
            ret = fun()
        else:
            ret = fun(arg)
        return ret

    def __call_children(self, children_lst, fun_name, arg=None):
        ret = True
        for child in children_lst:
            try:
                fun = getattr(child, fun_name)
            except AttributeError as e:
                raise NotImplementedError("Class `{}` does not implement `{}`"\
                        .format(child.__class__.__name__, fun_name))
            ret = ret and self.__call_child(child, fun, arg)
            if not ret:
                self.log_error("Could not {} service {}".format(fun_name,
                                                        child.get_name()))
        return ret

    """
    ###############
    Service
    ###############
    """

    def _start_impl(self):
        """ Services must be configured before start """
        self.log_info("App starting")
        self.load_children_conf()
        i_ret = self.__call_children(self.interactors, "start")
        g_ret = self.__call_children(self.guis, "start")
        h_ret = self.__call_children(self.handlers, "start")
        r_ret = self.__call_children(self.readers, "start")
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App started")
            return True
        self.log_warning("Some app services did not start")
        return False

    def _stop_impl(self):
        self.log_info("App stopping")
        r_ret = self.__call_children(self.readers, "stop")
        h_ret = self.__call_children(self.handlers, "stop")
        g_ret = self.__call_children(self.guis, "stop")
        i_ret = self.__call_children(self.interactors, "stop")
        self._write_conf()
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App stopped")
            return True
        self.log_warning("Some app services did not stop")
        return False

    def _pause_impl(self):
        r_ret = self.__call_children(self.readers, "pause")
        h_ret = self.__call_children(self.handlers, "pause")
        g_ret = self.__call_children(self.guis, "pause")
        i_ret = self.__call_children(self.interactors, "pause")
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App paused")
            return True
        self.log_warning("Some app services did not pause")
        return False

    def _resume_impl(self):
        i_ret = self.__call_children(self.interactors, "resume")
        g_ret = self.__call_children(self.guis, "resume")
        r_ret = self.__call_children(self.readers, "resume")
        h_ret = self.__call_children(self.handlers, "resume")
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App resumed")
            return True
        self.log_warning("Some app services did not resume")
        return False

    def _reset_impl(self):
        i_ret = self.__call_children(self.interactors, "reset")
        g_ret = self.__call_children(self.guis, "reset")
        r_ret = self.__call_children(self.readers, "reset")
        h_ret = self.__call_children(self.handlers, "reset")
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App is reset")
            return True
        self.log_warning("Some app services did not reset")
        return False


    def pause_readers(self):
        for reader in self.readers:
            reader.pause()

    def resume_readers(self):
        for reader in self.readers:
            reader.resume()

    def remove_reader(self, reader):
        if reader.is_active():
            reader.stop()
        self.readers.remove(reader)

    def remove_handler(self, handler):
        if handler.is_active():
            handler.stop()
        self.handlers.remove(handler)

    def remove_gui(self, gui):
        if gui.is_active():
            gui.stop()
        self.guis.remove(gui)

    def remove_interactor(self, interactor):
        if interactor.is_active():
            interactor.stop()
        self.interactors.remove(interactor)

    def add_reader(self, reader):
        self.readers.add(reader)
        reader.set_conf_obj(self.get_conf_obj())

    def add_handler(self, handler):
        self.handlers.add(handler)
        handler.set_conf_obj(self.get_conf_obj())

    def add_gui(self, gui):
        self.guis.add(gui)
        gui.set_conf_obj(self.get_conf_obj())

    def add_interactor(self, interactor):
        self.interactors.add(interactor)
        interactor.set_conf_obj(self.get_conf_obj())

    def service_state_changed(self, service, stopped, paused):
        return

    def emergency_backup(self, err):
        return

    def is_reader(self, service):
        return isinstance(service, Readers.IReader)

    def is_handler(self, service):
        return isinstance(service, Handlers.IHandler)

    def is_gui(self, service):
        return isinstance(service, GUI.IGui)

    def is_interactor(self, service):
        return isinstance(service, Interactors.IInteractors)

    """
    ###############
    Loop
    ###############
    """

    def set_loop(self, fun):
        if self.is_running():
            return False
        self._loop_impl = fun
        return True

    def set_timed_loop(self, sec):
        if self.is_running():
            return False
        self._max_sec_timed_loop = sec
        self._loop_impl = self._timed_loop
        return True

    def _timed_loop(self, timeout=None):
        max_sec = timeout or self._max_sec_timed_loop
        try:
            i = 0
            while i < max_sec and self.is_running():
                time.sleep(1)
                i += 1
        except KeyboardInterrupt:
            pass
        return self.stop()

    def _infinite_loop(self, timeout=None):
        ret = True
        start = time.time()
        now = start
        try:
            while self.is_running():
                time.sleep(1)
                now += 1
                if timeout is not None and now >= (start + timeout):
                    break
        except KeyboardInterrupt:
            pass
        return self.stop()

    def loop(self, timeout=None):
        return self._loop_impl(timeout=timeout)
