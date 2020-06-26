#!/usr/bin/python
# coding: utf-8

#
# System
#
import os
import sys
import time
import argparse

try:
    import ConfigParser
except ImportError:
    import configparser
    ConfigParser = configparser

#
# Sihd
#
import sihd

from sihd.Readers.AReader import AReader
from sihd.Handlers.AHandler import AHandler
from sihd.GUI.AGui import AGui
from sihd.Interactors.AInteractor import AInteractor
from sihd.Core.SihdService import SihdService
from sihd.Core.IService import IService
from sihd.Core.AConfigurable import AConfigurable
from sihd.Core.Runnable import Runnable

class SihdApp(SihdService):

    def __init__(self, name="SihdApp", args=None,
                    path=None, conf_path=None,
                    *pargs, **pkwargs):
        super().__init__(name, *pargs, **pkwargs)
        #Log
        self._default_log_level = "info"
        #Path
        self._path = path or SihdApp.get_sihd_path()
        self._conf_path = conf_path
        #Services
        self.readers = set()
        self.handlers = set()
        self.guis = set()
        self.interactors = set()
        #Args
        self.args = None
        self.__args_setted = None
        self.__children_configured = False
        self.__services_conf = ([], {})
        if args:
            self.set_args(args)
        self.pid = os.getpid()
        sihd.log.setup(self._default_log_level)

    def setup_app(self, *args, conf_path=None, **kwargs):
        self.log_debug("Starting application setup")
        self.__services_conf = (args, kwargs)
        #App service setup
        ret = self.load_app_conf(conf_path) is not False
        #Save app children's configuration
        ret = ret and self.save_children_conf()
        #Call children's service setup
        ret = ret and self.load_children_conf()
        #Link children's channels after 
        if ret is True:
            self.log_debug("Application successfully setup")
        else:
            self.log_error("Application setup has failed")
        return ret

    def post_setup(self):
        #App internal services setup
        ret = super().post_setup()
        args, kwargs = self.__services_conf
        return ret and self.build_services(*args, **kwargs) is not False

    def build_services(self):
        """
            To be implemented by children app
            -> Create app's services
        """
        pass

    #
    # Argvs
    #

    def build_args(self, parser):
        """
            To be implemented by children
            -> Add args to ArgumentParser
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
        return None

    def set_args(self, args):
        """ args = ['--foo', 'BAR'] - Bypass command line args """
        lst = None
        if isinstance(args, str):
            lst = args.strip().split(" ")
        elif isinstance(args, list):
            lst = args
        self.__args_setted = lst

    def parse_args(self):
        """ To be called by children - Useful at setup implementation """
        if self.args is not None:
            return self.args
        parser = argparse.ArgumentParser(prog=self.get_name(),
                                            conflict_handler='resolve')
        self.build_args(parser)
        if self.__args_setted is not None:
            self.args, unknown = parser.parse_known_args(args=self.__args_setted)
        else:
            self.args, unknown = parser.parse_known_args()
        if unknown:
            self.log_debug("Unknown args: {}".format(unknown))
        return self.args

    #
    # Configurations
    #

    def __setup_logger_conf(self, config):
        if self._path is None:
            sihd.log.error("No path set for app")
            return
        config.add_section("Logger")
        config.set("Logger", "level", self._default_log_level)
        config.set("Logger", "directory", os.path.join(self._path, "logs"))

    def __apply_conf(self, path):
        """ Load and apply conf from path and setup itself """
        self._conf_path = path
        obj = ConfigParser.ConfigParser()
        if not os.path.isfile(path):
            sihd.log.info("Making conf file {}".format(self.get_conf_path()))
            self.__setup_logger_conf(obj)
        else:
            obj.read(path)
            if not obj.has_section("Logger"):
                self.__setup_logger_conf(obj)
        sihd.log.add_file_handler(self.get_name(),
                                    directory=obj.get("Logger", "directory"),
                                    level=obj.get("Logger", "level"))
        self.log_debug("Logger is setup")
        #Setup
        ret = False
        try:
            ret = self.setup(obj)
        except Exception as e:
            self.log_error(e)
            self.log_error(sihd.get_traceback())
        return ret

    def load_app_conf(self, path=None):
        """ Load conf from path or <APP_PATH>/config/<APP_NAME>.ini """
        if path is None:
            filename = self.get_name() + ".ini"
            #conf = os.path.join(os.getcwd(), "config", filename)
            conf = os.path.join("config", filename)
            path = os.path.join(self._path, conf)
        return self.__apply_conf(path)

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
            self.log_error(sihd.get_traceback())
            return False
        return True

    def load_children_conf(self):
        if self.__children_configured:
            self.log_warning("Services already configured")
            return False
        self.__children_configured = True
        conf = self.get_conf_obj()
        ret = self.call_children('setup', AConfigurable, args=[conf])
        if ret:
            self.log_info("Services are configured")
        else:
            self.log_warning("Some services could not be configured")
        return ret

    def save_children_conf(self):
        ret = self.call_children('save_conf', AConfigurable)
        self.log_info("Services configurations are saved")
        conf = self.get_conf_obj()
        return self._write_conf(conf)

    #
    # Utils
    #

    def set_cwd_path(self):
        self.set_app_path(os.getcwd())

    def set_module_path(self, module, parent=False):
        path = os.path.dirname(module.__file__)
        if parent is True:
            path = os.path.dirname(path)
        self.set_app_path(path)

    @staticmethod
    def get_sihd_path():
        return os.path.dirname(os.path.dirname(sihd.__file__))

    def set_app_path(self, path):
        self._path = path

    def get_app_path(self):
        return self._path

    def get_service(self, name):
        child = self.get_child(name)
        if isinstance(child, IService):
            return child
        return None

    def get_sihd_service(self, name):
        child = self.get_child(name)
        if isinstance(child, SihdService):
            return child
        return None

    #
    # Service
    #

    def add_state_observer(self, service):
        state = service.get_channel("service_state")
        if not state:
            self.log_error("Service {}: could not find "
                    "service_state channel to observe".format(service.get_name()))
            return False
        input_lst = self.get_channels_input()
        if state not in input_lst:
            input_lst.append(state)
        self.log_debug("Service {}: added state observation"\
                        .format(service.get_name()))
        return True

    def remove_state_observer(self, service):
        state = service.get_channel("service_state")
        if not state:
            self.log_error("Could not find service {} "
                    "state channel to remove observation".format(service))
            return False
        input_lst = self.get_channels_input()
        to_remove = -1
        for i, channel in enumerate(input_lst):
            if channel == state:
                to_remove = i
                break
        if to_remove == -1:
            self.log_error("Could not find service {} "
                    "state channel in observations for removal")
        else:
            input_lst.pop(to_remove)
            self.log_debug("Stopped service {} state observation"\
                            .format(service))

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
            st_readers[reader.get_name()] = reader.get_service_state_str()
        for handler in self.handlers:
            st_handlers[handler.get_name()] = handler.get_service_state_str()
        for gui in self.guis:
            st_guis[gui.get_name()] = gui.get_service_state_str()
        for interactor in self.interactors:
            st_interactors[interactor.get_name()] = interactor.get_service_state_str()
        return status

    #
    # Init
    #

    def _init_impl(self):
        """ Services must be configured before start """
        self.log_info("App initialising")
        ret = self.call_children('init', IService)
        if not ret:
            self.log_warning("Some app services did not init")
        return ret

    #
    # Start
    #

    def start_readers(self):
        ret = self.call_children("start", AReader)
        if ret:
            self.log_info("App readers started")
        else:
            self.log_warning("Some app readers did not start")
        return ret

    def start_interactors(self):
        ret = self.call_children("start", AInteractor)
        if ret:
            self.log_info("App interactors started")
        else:
            self.log_warning("Some app interactors did not start")
        return ret

    def start_handlers(self):
        ret = self.call_children("start", AHandler)
        if ret:
            self.log_info("App handlers started")
        else:
            self.log_warning("Some app handlers did not start")
        return ret

    def start_all(self):
        """ Default start option """
        return self.call_children('start', IService)

    def start_order(self):
        """ Application's services start entry point """
        return self.start_all()

    def _start_impl(self):
        self.log_info("App starting")
        if self.start_order() is False:
            self.log_error("failed to start services")
        return True

    #
    # Stop
    #

    def _stop_impl(self):
        self.log_info("App stopping")
        ret = super()._stop_impl()
        self._write_conf()
        if not ret:
            self.log_warning("Some app services did not stop")
        return ret

    #
    # Reset
    #

    def _reset_impl(self):
        ret = super()._reset_impl()
        if ret:
            self.__children_configured = False
        return ret

    #
    # Actions on services
    #

    def pause_readers(self):
        return self.call_children('pause', AReader)

    def resume_readers(self):
        return self.call_children('resume', AReader)

    def pause_interactors(self):
        return self.call_children('pause', AInteractor)

    def resume_interactors(self):
        return self.call_children('pause', AInteractor)

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

    #
    # Adding services
    #

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

    #
    # SihdServices getters
    #

    def get_readers(self):
        return self.readers

    def get_handlers(self):
        return self.handlers

    def get_guis(self):
        return self.guis

    def get_interactors(self):
        return self.interactors

    #
    # Checks
    #

    @staticmethod
    def is_service(service):
        return isinstance(service, IService)

    @staticmethod
    def is_sihd_service(service):
        return isinstance(service, SihdService)

    @staticmethod
    def is_reader(service):
        return isinstance(service, AReader)

    @staticmethod
    def is_handler(service):
        return isinstance(service, AHandler)

    @staticmethod
    def is_gui(service):
        return isinstance(service, AGui)

    @staticmethod
    def is_interactor(service):
        return isinstance(service, AInteractor)

    #
    # Backup
    #
    
    def emergency_backup(self, err):
        return

    #
    # Channels
    #

    def _pre_handle(self, channel):
        if channel.get_name() == "service_state":
            service = channel.get_parent()
            stopped, paused = service.get_service_state()
            self.service_state_changed(service, stopped, paused)
            return True
        return False
        
    def service_state_changed(self, service, stopped, paused):
        pass

    #
    # Loop
    #

    def on_loop_start(self, runnable):
        self.log_debug("App loop started")

    def on_loop_stop(self, runnable, i):
        self.log_debug("App loop stopped after {} iterations".format(i))

    def on_loop_error(self, runnable, i, err):
        self.log_error(err)
        self.log_error(sihd.get_traceback())

    def _loop_impl(self, timeout=None, frequency=None, steps=None):
        runnable = Runnable(name='loop',
                            step=self.step,
                            on_start=self.on_loop_start,
                            on_stop=self.on_loop_stop,
                            on_err=self.on_loop_error,
                            frequency=frequency or 10,
                            timeout=timeout or 0,
                            max_iter=steps or 0,
                            parent=self)
        runnable.run()
        self.remove_child(runnable)

    def on_step(self):
        pass

    def step(self):
        self.read_channels_input()
        return self.on_step()

    def loop(self, *args, **kwargs):
        return self._loop_impl(*args, **kwargs)
