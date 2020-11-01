#!/usr/bin/python
# coding: utf-8

__all__ = ('SihdApp')

#
# System
#
import os
import sys
import time
import argparse
import json

#
# Sihd
#
import sihd

from sihd.readers.AReader import AReader
from sihd.handlers.AHandler import AHandler
from sihd.gui.AGui import AGui
from sihd.interactors.AInteractor import AInteractor
from sihd.core.SihdObject import SihdObject
from sihd.core.IService import IService
from sihd.core.AConfigurable import AConfigurable
from sihd.core.Runnable import Runnable

class SihdApp(SihdObject):

    def __init__(self, name="SihdApp", args=None, path=None,
                    conf_path=None, *pargs, **pkwargs):
        super().__init__(name, *pargs, **pkwargs)
        self.configuration.add_defaults({
            'log': {
                'level': 'info',
                'file_level': 'debug',
                'dir': None
            }
        })
        #Path
        self._path = path or sihd.get_path()
        self._conf_path = conf_path
        #Services
        self.readers = set()
        self.handlers = set()
        self.guis = set()
        self.interactors = set()
        #Args
        self.args = None
        self.__args_setted = None
        self.__services_conf = ([], {})
        if args:
            self.set_args(args)
        self.pid = os.getpid()
        sihd.log.setup("info")

    def setup(self, conf=None):
        """ If no conf is provided, load default conf """
        if conf is None:
            conf = self.read_app_conf(self._conf_path)
        ret = self.__setup_file_logger(conf)
        return ret and super().setup(conf)

    def post_setup(self):
        ret = super().post_setup()
        args, kwargs = self.__services_conf
        self.log_debug("Building services")
        ret = ret and self.build_services(*args, **kwargs) is not False
        if not ret:
            self.log_error("Error building services")
        return ret

    def setup_app(self, *args, conf_path=None, conf=None, **kwargs):
        """
            Any args provided to this method will be relayed in
                build_services method

            Args not relayed:
                :param conf_path: Path to app's configuration
                :param conf: Dictionnary of app's configuration
        """
        self.log_debug("Entering application's setup")
        self.__services_conf = (args, kwargs)
        if conf_path is not None:
            self._conf_path = conf_path
        return self.setup(conf)

    def on_setup(self, conf):
        sihd.log.set_level(conf.get('log')['level'])
        return super().on_setup(conf)

    def build_services(self, *args, **kwargs):
        """
            Create and configure app's services
            To be implemented by applications
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

    def __setup_file_logger(self, conf=None):
        directory = os.path.join(self.get_app_path(), "logs")
        level = self.configuration.get('log')['file_level']
        if conf is not None:
            log_conf = conf.get('log', None)
            if log_conf is not None:
                if 'dir' in log_conf:
                    directory = log_conf['dir']
                else:
                    log_conf['dir'] = directory
                if 'file_level' in log_conf:
                    level = log_conf['file_level']
                else:
                    log_conf['file_level'] = level
        sihd.log.add_file_handler(self.get_name(),
                                    directory=directory,
                                    level=level)
        self.log_debug("File logger is setup: {}".format(directory))
        return True

    def read_app_conf(self, path=None):
        """ Load conf from path or <APP_PATH>/config/<APP_NAME>.ini """
        conf = None
        if self._conf_path is None:
            self._conf_path = self.get_default_conf_path()
        path = path or self._conf_path
        sihd.log.info("Reading configuration file: {}".format(path))
        if not os.path.isfile(path):
            # Write initial conf
            conf = self.get_conf()
            self.write_conf(conf, path)
        else:
            # Read conf
            try:
                with open(path, 'r') as configfile:
                    conf = json.load(configfile)
            except IOError as e:
                self.log_error("Could not read app configuration: {}".format(e))
                self.log_error(sihd.sys.get_traceback())
        self._conf_path = path
        return conf

    def get_default_conf_path(self):
        filename = self.get_name() + ".ini"
        config_dir = os.path.join("config", filename)
        return os.path.join(self.get_app_path(), config_dir)

    def get_conf_path(self):
        return self._conf_path

    def write_conf(self, conf, path=None):
        path = path or self.get_conf_path()
        directory = os.path.dirname(path)
        if not os.path.exists(directory):
            try:
                os.makedirs(directory)
            except Exception as e:
                self.log_error("Could not make directories"
                                " for app configuration directory: {}".format(e))
                return False
        self.log_info("Writing configuration file: {}".format(path))
        try:
            with open(path, 'w+') as configfile:
                json.dump(conf, configfile)
        except IOError as e:
            self.log_error("Could not write app configuration: {}".format(e))
            self.log_error(sihd.sys.get_traceback())
            return False
        return True

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
        if isinstance(child, SihdObject):
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
        self.log_info("App initializing")
        ret = self.call_children('init', cls=IService)
        if not ret:
            self.log_warning("Some app services did not init")
        return ret

    #
    # Start
    #

    def start_readers(self):
        ret = self.call_children("start", cls=AReader)
        if ret:
            self.log_info("App readers started")
        else:
            self.log_warning("Some app readers did not start")
        return ret

    def start_interactors(self):
        ret = self.call_children("start", cls=AInteractor)
        if ret:
            self.log_info("App interactors started")
        else:
            self.log_warning("Some app interactors did not start")
        return ret

    def start_handlers(self):
        ret = self.call_children("start", cls=AHandler)
        if ret:
            self.log_info("App handlers started")
        else:
            self.log_warning("Some app handlers did not start")
        return ret

    def start_all(self):
        """ Default start option """
        return self.call_children('start', cls=IService)

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
        self.write_conf(self.get_conf())
        if not ret:
            self.log_warning("Some app services did not stop")
        return ret

    #
    # Reset
    #

    def _reset_impl(self):
        ret = super()._reset_impl()
        return ret

    #
    # Actions on services
    #

    def pause_readers(self):
        return self.call_children('pause', cls=AReader)

    def resume_readers(self):
        return self.call_children('resume', cls=AReader)

    def pause_interactors(self):
        return self.call_children('pause', cls=AInteractor)

    def resume_interactors(self):
        return self.call_children('pause', cls=AInteractor)

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

    def add_handler(self, handler):
        self.handlers.add(handler)

    def add_gui(self, gui):
        self.guis.add(gui)

    def add_interactor(self, interactor):
        self.interactors.add(interactor)

    #
    # SihdObjects getters
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
        return isinstance(service, SihdObject)

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
        self.log_error(sihd.sys.get_traceback())

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
