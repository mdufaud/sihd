#!/usr/bin/python
# coding: utf-8

#
# System
#
from typing import Union, Tuple
subprocess = None
shlex = None

from sihd.Interactors.AInteractor import AInteractor

class ShellInteractor(AInteractor):

    def __init__(self, name="ShellInteractor", **kwargs):
        super().__init__(name=name, **kwargs)
        global subprocess
        if subprocess is None:
            import subprocess
        global shlex
        if shlex is None:
            import shlex
        self.set_default_conf({
            "cmd": "/your/cmd --arg",
            "pipe": "ex: stdin;stdout",
            "devnull": "ex: stderr;stdin",
            "timeout_communication": 0.5,
            "input_data": "",
            "stderr_to_out": False,
        })
        self.__exec = None
        self.__input = None
        self.__args = {}
        self.__timeout = None
        self.__proc = None
        self.add_channel_input("stdin")

    #
    # Configuration
    #

    def on_setup(self):
        ret = super().on_setup()
        cmd = self.get_conf("cmd", default=False)
        pipe = self.get_conf("pipe", default=False)
        stderr = self.get_conf("stderr_to_out")
        devnull = self.get_conf("devnull", default=False)
        input_data = self.get_conf("input_data")
        self.set_timeout(self.get_conf("timeout_communication"))
        if cmd:
            self.set_cmd(cmd)
        if pipe:
            self.set_pipe(pipe.split(';'))
        if stderr:
            self.set_stderr_out()
        if devnull:
            self.set_devnull(devnull.split(';'))
        if input_data:
            self.set_input(input_data)
        return True

    def set_stdin(self, std):
        self.__args['stdin'] = std

    def set_stdout(self, fd):
        self.__args['stdout'] = fd

    def set_stderr(self, fd):
        self.__args['stderr'] = fd

    def set_timeout(self, timeout):
        self.__timeout = timeout

    def set_input(self, data):
        if isinstance(data, bool):
            if data is False:
                self.set_stdin(None)
                self.__input = None
            return
        if isinstance(data, str):
            data = data.encode()
        if isinstance(data, bytes):
            self.set_stdin_pipe()
            self.__input = data
        else:
            raise ValueError("Stdin data is not bytes: {}".format(data))

    def set_cmd(self, cmd: Union[str, list]) -> list:
        if isinstance(cmd, str):
            cmd = self._get_cmd_from_str(cmd)
        elif not isinstance(cmd, (list, tuple, set)):
            raise ValueError("Command unrecognized: {}".format(cmd))
        self.__exec = cmd
        return cmd

    def _get_cmd_from_str(self, cmd: str):
        """ Get an argument list from string (shell parsing) """
        return shlex.split(cmd)

    #
    # Channels
    #

    def handle(self, channel):
        if channel == self.stdin:
            data = channel.read()
            if data is not None:
                self.set_input(data)

    #
    # Interactor
    #

    def on_new_interaction(self, cmd: Union[str, list]) -> list:
        return self.set_cmd(cmd)

    def on_interaction(self, cmd, *args, **kwargs):
        """ Executes command and close children """
        child = self.execute(cmd)
        if child is None:
            return False
        out, err, timedout = self.communicate()
        self.set_result((child.returncode, out, err, timedout))
        return child.returncode == 0

    #
    # Cmd execution
    #

    def exe(self, cmd: str) -> bool:
        """ For a fast single line execution """
        self.set_cmd(cmd)
        child = self.execute()
        if child is None:
            return False
        self.communicate()
        return child.returncode == 0

    def execute(self, cmd: list = None) -> subprocess:
        """
            Wrapper around Popen builder.

            :return: Popen object or None if failed
        """
        if cmd is None:
            cmd = self.__exec
        ret = None
        try:
            ret = subprocess.Popen(cmd, **self.__args)
        except OSError as e:
            self.log_error("OSError executing: {} "
                            "(cmd: {})".format(e, " ".join(cmd)))
        except ValueError as e:
            self.log_error("ValueError executing: {} "
                            "(cmd: {})".format(e, " ".join(cmd)))
        if ret:
            self.__proc = ret
        return ret

    def communicate(self, input=None, timeout=None) -> Tuple[bytes, bytes]:
        """
            Use a Popen proc object and applies communicate on it

            :return: stdout and stderr as bytes
        """
        if timeout is None:
            timeout = self.__timeout
        if input is None:
            input = self.__input
        timedout = False
        try:
            out, errs = self.__proc.communicate(timeout=timeout,
                                                input=input)
        except subprocess.TimeoutExpired:
            timedout = True
            self.log_error("Timed out communication ({})".format(timeout))
            out, errs = self.end_process(kill=True)
        if errs == b"":
            errs = None
        self.__proc = None
        return out, errs, timedout

    def end_process(self, kill=False):
        """ Kill the children process by applying communicate on it """
        if kill:
            self.__proc.kill()
        out, errs = self.__proc.communicate()
        self.__proc = None
        return out, errs

    def get_proc(self):
        return self.__proc

    def get_args(self):
        return self.__exec

    #
    # /dev/null
    #

    def set_devnull(self, lst):
        """ @param lst accepts 'stdin, 'stdout' or 'stderr' """
        for std in lst:
            if std == "stdin":
                self.set_stdin_devnull()
            elif std == "stdout":
                self.set_stdout_devnull()
            elif std == "stderr":
                self.set_stderr_devnull()
            else:
                self.log_warning("No such std {} for devnull".format(std))

    def set_stdin_devnull(self):
        self.__args['stdin'] = subprocess.DEVNULL

    def set_stdout_devnull(self):
        self.__args['stdout'] = subprocess.DEVNULL

    def set_stderr_devnull(self):
        self.__args['stderr'] = subprocess.DEVNULL

    #
    # Pipe
    #

    def set_pipe(self, lst):
        """ @param lst accepts 'stdin, 'stdout' or 'stderr' """
        for std in lst:
            if std == "stdin":
                self.set_stdin_pipe()
            elif std == "stdout":
                self.set_stdout_pipe()
            elif std == "stderr":
                self.set_stderr_pipe()
            else:
                self.log_warning("No such std {} for pipe".format(std))

    def set_stdin_pipe(self):
        self.__args['stdin'] = subprocess.PIPE

    def set_stdout_pipe(self):
        self.__args['stdout'] = subprocess.PIPE

    def set_stderr_pipe(self):
        self.__args['stderr'] = subprocess.PIPE

    #
    # Stderr
    #

    def set_stderr_out(self):
        """ Routes stderr to stdout """
        self.__args['stderr'] = subprocess.STDOUT
