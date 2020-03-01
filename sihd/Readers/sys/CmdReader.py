#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os
import sys
import select

from sihd.Readers.IReader import IReader
from sihd.Core.IObserver import IObserver
from sihd.Interactors.sys.CmdInteractor import CmdInteractor

class CmdReader(IReader, IObserver):

    def __init__(self, path=None, app=None, name="CmdReader"):
        super(CmdReader, self).__init__(app=app, name=name)
        self._interactor = CmdInteractor()
        self._set_default_conf({
            "cmd": "/your/cmd --your=args",
            "pipe": "stdout;stderr",
            "devnull": "",
            "timeout": 0.2,
            "input_data": "",
            "stderr_to_out": False,
            "handle_process": True,
            'thread_frequency': 10,
        })
        self.set_step_method(self.diffuse_execution)
        self._handle_proc = True
        self._cmd = ""
        self._last_proc = None
        self.__pipe_reader = None

    """ IConfigurable """

    def _setup_impl(self):
        super(CmdReader, self)._setup_impl()
        cmd = self.get_conf("cmd", default=False)
        if cmd:
            self.set_cmd(cmd)
        pipe = self.get_conf("pipe")
        if pipe:
            self.set_pipe(pipe.split(';'))
        input_data = self.get_conf("input_data")
        if input_data:
            self.set_pipe(["stdin"])
            self.set_input(input_data)
        stderr = self.get_conf("stderr_to_out")
        if stderr:
            self.set_stderr_out()
        devnull = self.get_conf("devnull")
        if devnull:
            self.set_devnull(devnull.split(';'))
        timeout = self.get_conf("timeout")
        if timeout:
            self.set_timeout(timeout)
        handle = self.get_conf("handle_process")
        if handle:
            self.set_handle_process(handle)
        return True

    def set_handle_process(self, active):
        self._handle_proc = active

    def set_devnull(self, lst):
        self._interactor.set_devnull(lst)

    def set_pipe(self, lst):
        self._interactor.set_pipe(lst)

    def set_input(self, data):
        self._interactor.set_input(data)

    def set_cmd(self, cmd):
        self._cmd = cmd
        self._interactor.set_cmd(cmd)

    def set_timeout(self, timeout):
        self._interactor.set_timeout(timeout)

    def set_stderr_out(self):
        self._interactor.set_stderr_out()

    """ Reader """

    def get_stdout(self):
        proc = self._interactor.get_proc()
        if proc:
            return proc.stdout
        return None

    def get_stderr(self):
        proc = self._interactor.get_proc()
        if proc:
            return proc.stdout
        return None

    def get_stdin(self):
        proc = self._interactor.get_proc()
        if proc:
            return proc.stdin
        return None

    def get_returncode(self):
        proc = self._interactor.get_proc()
        if proc:
            return proc.returncode
        return None

    def end_process(self, kill=False):
        self._interactor.end_process(kill=kill)

    def __clear_last_proc(self):
        proc = self._last_proc
        if proc and proc.returncode is None:
            self.log_warning("Last process was not handled")
            self.end_process()
        self._last_proc = None

    def configure_pipe_cmd_reader(self, reader):
        if isinstance(reader, CmdReader):
            reader.add_observer(self)
            reader.set_handle_process(False)
            reader.set_pipe(['stdout', 'stderr'])
            self.__pipe_reader = reader
            self.pause()
        else:
            self.log_warning("Incompatible reader {} "
                            "for pipes".format(reader.get_name()))

    def on_notify(self, reader, proc):
        """ Permits CmdReader chaining """
        if not self.is_running():
            return
        err = True
        if reader != self.__pipe_reader:
            self.log_error("Handling wrong reader: {}".format(reader.get_name()))
        elif proc and proc.stdout:
            self._interactor.set_stdin(proc.stdout)
            self.resume()
            reader.pause()
            err = False
        else:
            self.log_error("Wrong value type for handling")
        if err:
            self.stop()
        return not err

    def execute(self):
        self.__clear_last_proc()
        itr = self._interactor
        ret = None
        if itr:
            proc = itr.execute()
            if self._handle_proc is True:
                out, errs = itr.communicate()
                ret = out if errs is None else None
            else:
                ret = proc
            self._last_proc = proc
        return ret

    def diffuse_execution(self):
        self.__clear_last_proc()
        proc = self._interactor.execute()
        if proc:
            if self._handle_proc is False:
                self.deliver(proc)
            else:
                out, errs = self._interactor.communicate()
                if errs is None:
                    self.deliver(out)
                else:
                    self.notify_error(errs)
                    self.log_error("Error handling command "
                                "'{}': {}".format(self._cmd, errs.decode()))
                    proc = None
            self._last_proc = proc
            if self.__pipe_reader:
                self.__pipe_reader.end_process()
                self.__pipe_reader.resume()
                self.pause()
        else:
            self.log_error("Error in command '{}'".format(self._cmd))
        return proc is not None
