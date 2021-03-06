#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time
import argparse

try:
    import ConfigParser
except ImportError:
    import configparser
    ConfigParser = configparser

import sihd
from sihd import Core, Readers, Handlers, GUI, Interactors

class IApp(Core.IService, Core.ILoggable,
            Core.IConfigurable, Core.IDumpable,
            Core.IServiceStateObserver):

    def __init__(self, name="IApp", *args, **kwargs):
        super(IApp, self).__init__(name, *args, **kwargs)
        #Path
        self._path = None
        self._path = IApp.get_sihd_path()
        self._conf_path = None
        #Services
        self.readers = set()
        self.handlers = set()
        self.guis = set()
        self.interactors = set()
        self._children_configured = False
        #App
        self._loop_impl = self._infinite_loop
        #Args
        self.args = None
        self.__args_setted = None

    def setup_app(self, *args, **kwargs):
        ret = False
        self.log_debug("Starting application setup")
        self.load_app_conf()
        ret = self._setup_app_impl(*args, **kwargs)
        if ret is False:
            self.log_error("Application setup has failed")
        else:
            self.save_children_conf()
            self.log_debug("Application successfully setup")
        return ret

    def _setup_app_impl(self):
        """ To be implemented by children """
        return False

    """
    ###############
    ARGS MANAGEMENT
    ###############
    """

    def define_args(self, parser):
        """
            To be implemented by children
            Should add args to ArgumentParser
        """
        return None

    def is_args(self):
        if len(sys.argv) == 1:
            return False
        return True

    def get_arg(self, name):
        args = self.args
        if args:
            return vars(args).get(name, None)
        return self.get_conf(name)

    def set_args(self, args):
        """ args = ['--foo', 'BAR'] - Bypass command line args """
        lst = None
        if isinstance(args, str):
            lst = args.strip().split(" ")
        elif isinstance(args, list):
            lst = args
        self.__args_setted = lst

    def parse_args(self):
        parser = argparse.ArgumentParser(prog=self.get_name(),
                                            conflict_handler='resolve')
        self.define_args(parser)
        if self.__args_setted is not None:
            self.args, unknown = parser.parse_known_args(args=self.__args_setted)
        else:
            self.args, unknown = parser.parse_known_args()
        if unknown:
            self.log_debug("Unknown args: {}".format(unknown))
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
            path = to_read
        return self.__apply_conf(path)

    def __setup_logger_conf(self, config):
        if self._path is None:
            self.error("No path set for app")
            return
        config.add_section("Logger")
        config.set("Logger", "level", "info")
        config.set("Logger", "directory", os.path.join(self._path, "logs"))

    def __apply_conf(self, path):
        """ Load and apply conf from path and setup itself """
        self._conf_path = path
        obj = ConfigParser.ConfigParser()
        if not os.path.isfile(path):
            self.say("Making conf file {}".format(self.get_conf_path()))
            self.__setup_logger_conf(obj)
        else:
            obj.read(path)
            if not obj.has_section("Logger"):
                self.__setup_logger_conf(obj)
        Core.ILoggable.setup_log(self.get_name(),
                                    obj.get("Logger", "level"),
                                    obj.get("Logger", "directory"))
        self.log_debug("Logger is setup")
        ret = False
        try:
            ret = self.setup(obj)
        except Exception as e:
            self.log_error(e)
        return ret

    def get_conf_path(self):
        return self._conf_path

    def _write_conf(self, obj=None):
        obj = self.get_conf_obj() if obj is None else obj
        if obj is None:
            self.log_error("No object to write configuration to")
            return False
        path = self.get_conf_path()
        directory = os.path.dirname(path)
        if not os.path.exists(directory):
            try:
                os.makedirs(directory)
            except Exception as e:
                self.log_error("Could not make directories"
                                " for app configuration directory: {}".format(e))
                return
        try:
            with open(path, 'w+') as configfile:
                obj.write(configfile)
                self.log_debug("Conf file {} is written".format(path))
        except IOError as e:
            self.log_error("Could not write app configuration: {}".format(e))
            return False
        return True

    def load_children_conf(self):
        if self._children_configured:
            self.log_debug("Services already configured")
            return False
        self._children_configured = True
        conf = self.get_conf_obj()
        fun = self.__call_children
        ret_i = fun(self.interactors, "setup", conf)
        ret_g = fun(self.guis, "setup", conf)
        ret_h = fun(self.handlers, "setup", conf)
        ret_r = fun(self.readers, "setup", conf)
        if ret_i and ret_g and ret_h and ret_r:
            self.log_info("Services are configured")
            return True
        self.log_warning("Some services could not be configured")
        return False

    def save_children_conf(self):
        fun = self.__call_children
        fun(self.interactors, "save_conf")
        fun(self.guis, "save_conf")
        fun(self.handlers, "save_conf")
        fun(self.readers, "save_conf")
        self.log_info("Services configurations are saved")
        conf = self.get_conf_obj()
        self._write_conf(conf)
        return True

    """
    ###############
    Utils
    ###############
    """

    def set_cwd_path(self):
        self.set_path(os.getcwd())

    def set_module_path(self, module):
        self.set_path(os.path.dirname(module.__file__))

    @staticmethod
    def get_sihd_path():
        return os.path.dirname(sihd.__file__)

    def set_path(self, path):
        self._path = path

    def get_path(self):
        return self._path

    def get_service(self, name):
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
        return None

    def __call_child(self, child, fun, arg=None):
        ret = False
        if arg is None:
            ret = fun()
        else:
            ret = fun(arg)
        return ret

    def __call_children(self, children_lst, fun_name, arg=None, fail=True):
        ret = True
        for child in children_lst:
            try:
                fun = getattr(child, fun_name)
            except AttributeError as e:
                raise NotImplementedError("Class `{}` does not implement `{}`"\
                        .format(child.__class__.__name__, fun_name))
            ret = ret and self.__call_child(child, fun, arg)
            if not ret and fail:
                self.log_error("Could not {} for service {}".format(fun_name,
                                                            child.get_name()))
        return ret

    """
    ###############
    Service
    ###############
    """

    def get_services_status(self):
        st_readers = {}
        st_handlers = {}
        st_guis = {}
        st_interactors = {}
        status = {
            "readers": st_readers,
            "handlers": st_handlers,
            "guis": st_guis,
            "interactors": st_interactors,
        }
        for reader in self.readers:
            st_readers[reader.get_name()] = reader.get_service_state()
        for handler in self.handlers:
            st_handlers[handler.get_name()] = handler.get_service_state()
        for gui in self.guis:
            st_guis[gui.get_name()] = gui.get_service_state()
        for interactor in self.interactors:
            st_interactors[interactor.get_name()] = interactor.get_service_state()
        return status

    def start_readers(self):
        if self.__call_children(self.readers, "start"):
            self.log_info("App readers started")
            return True
        self.log_warning("Some app readers did not start")
        return False

    def start_interactors(self):
        if self.__call_children(self.interactors, "start"):
            self.log_info("App interactors started")
            return True
        self.log_warning("Some app interactors did not start")
        return False

    def start_all(self):
        return self.start() and self.start_readers() and self.start_interactors()

    def _start_impl(self):
        """ Services must be configured before start """
        self.log_info("App starting")
        ret = self.load_children_conf()
        if ret:
            fun = self.__call_children
            g_ret = fun(self.guis, "start")
            h_ret = fun(self.handlers, "start")
            if h_ret and g_ret:
                self.log_info("App started")
                return True
        self.log_warning("Some app services did not start")
        return False

    def _stop_impl(self):
        self.log_info("App stopping")
        fun = self.__call_children
        r_ret = fun(self.readers, "stop")
        h_ret = fun(self.handlers, "stop")
        g_ret = fun(self.guis, "stop")
        i_ret = fun(self.interactors, "stop")
        self._write_conf()
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App stopped")
            return True
        self.log_warning("Some app services did not stop")
        return False

    def _pause_impl(self):
        fun = self.__call_children
        r_ret = fun(self.readers, "pause")
        h_ret = fun(self.handlers, "pause")
        g_ret = fun(self.guis, "pause")
        i_ret = fun(self.interactors, "pause")
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App paused")
            return True
        self.log_warning("Some app services did not pause")
        return False

    def _resume_impl(self):
        fun = self.__call_children
        i_ret = fun(self.interactors, "resume")
        g_ret = fun(self.guis, "resume")
        r_ret = fun(self.readers, "resume")
        h_ret = fun(self.handlers, "resume")
        if r_ret and h_ret and g_ret and i_ret:
            self.log_info("App resumed")
            return True
        self.log_warning("Some app services did not resume")
        return False

    def _reset_impl(self):
        fun = self.__call_children
        i_ret = fun(self.interactors, "reset")
        g_ret = fun(self.guis, "reset")
        r_ret = fun(self.readers, "reset")
        h_ret = fun(self.handlers, "reset")
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

    def pause_interactors(self):
        for interactor in self.interactors:
            interactor.pause()

    def resume_interactors(self):
        for interactor in self.interactors:
            interactor.resume()

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

    def get_readers(self):
        return self.readers

    def get_handlers(self):
        return self.handlers

    def get_guis(self):
        return self.guis

    def get_interactors(self):
        return self.interactors

    @staticmethod
    def is_reader(service):
        return isinstance(service, Readers.IReader)

    @staticmethod
    def is_handler(service):
        return isinstance(service, Handlers.IHandler)

    @staticmethod
    def is_gui(service):
        return isinstance(service, GUI.IGui)

    @staticmethod
    def is_interactor(service):
        return isinstance(service, Interactors.IInteractor)

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

    def loop(self, timeout=None):
        return self._loop_impl(timeout=timeout)
