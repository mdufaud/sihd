#!/usr/bin/python
#coding: utf-8

""" System """


import socket

subprocess = None
shlex = None

from sihd.Interactors.IInteractor import IInteractor

class CmdInteractor(IInteractor):

    def __init__(self, app=None, name="CmdInteractor"):
        super(CmdInteractor, self).__init__(app=app, name=name)
        global subprocess
        if subprocess is None:
            import subprocess
        global shlex
        if shlex is None:
            import shlex
        self._set_default_conf({
            "cmd": "/your/cmd --your=args",
            "pipe": "ex: stdin;stdout",
            "devnull": "ex: stderr;stdin",
            "timeout": 0.1,
            "input_data": "",
            "stderr_to_out": False,
        })
        self._exec = []
        self._args = {}
        self._timeout = None
        self._input = None
        self._proc = None

    """ IConfigurable """

    def _setup_impl(self):
        super(CmdInteractor, self)._setup_impl()
        cmd = self.get_conf("cmd", default=False)
        if cmd:
            self.set_cmd(cmd)
        pipe = self.get_conf("pipe", default=False)
        if pipe:
            self.set_pipe(pipe.split(';'))
        stderr = self.get_conf("stderr_to_out")
        if stderr:
            self.set_stderr_out()
        devnull = self.get_conf("devnull", default=False)
        if devnull:
            self.set_devnull(devnull.split(';'))
        timeout = self.get_conf("timeout")
        if timeout:
            self.set_timeout(timeout)
        input_data = self.get_conf("input_data")
        if input_data:
            self.set_stdin_pipe()
            self.set_input(input_data)
        return True

    def set_stdin(self, std):
        self._args['stdin'] = std

    def set_stdout(self, fd):
        self._args['stdout'] = fd

    def set_stderr(self, fd):
        self._args['stderr'] = fd

    def set_timeout(self, timeout):
        self._timeout = timeout

    """ IInteractor """

    def _interact_impl(self, cmd, *args, **kwargs):
        return self.exe(cmd)

    """ Cmd """

    def set_cmd(self, cmd):
        """ @param cmd either a string or a list """
        if isinstance(cmd, str):
            self._get_cmd_from_str(cmd)
        elif isinstance(cmd, list):
            self._exec = cmd

    def _get_cmd_from_str(self, cmd):
        lst = shlex.split(cmd)
        self._exec = lst

    def exe(self, cmd=None):
        """ Executes command and close children """
        child = self.execute(cmd)
        if child is None:
            return False
        child.communicate()
        return child.returncode == 0

    def execute(self, cmd=None):
        """ Wrapper around Popen builder
            @return Popen object or None if failed
        """
        if cmd is not None:
            self.set_cmd(cmd)
        ret = None
        try:
            ret = subprocess.Popen(self._exec, **self._args)
        except OSError as e:
            self.log_error("OSError executing: {} "
                            "(cmd: {})".format(e, " ".join(self._exec)))
        except ValueError as e:
            self.log_error("ValueError executing: {} "
                            "(cmd: {})".format(e, " ".join(self._exec)))
        if ret:
            self._proc = ret
        return ret

    def communicate(self, input=None, timeout=None):
        """ Use a Popen proc object and applies communicate on it
            @return stdout and stderr (None if not failed) as binary
        """
        if timeout is None:
            timeout = self._timeout
        if input is None:
            input = self._input
        try:
            out, errs = self._proc.communicate(timeout=timeout,
                                                input=input)
        except subprocess.TimeoutExpired:
            self.log_error("Timed out communication ({})".format(timeout))
            out, errs = self.end_process(kill=True)
        if errs == b"":
            errs = None
        return out, errs

    def end_process(self, kill=False):
        """ Kill the children process by applying communicate on it """
        if kill:
            self._proc.kill()
        out, errs = self._proc.communicate()
        self._proc = None
        return out, errs

    def get_proc(self):
        return self._proc

    """ DEVNULL """

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
        self._args['stdin'] = subprocess.DEVNULL

    def set_stdout_devnull(self):
        self._args['stdout'] = subprocess.DEVNULL

    def set_stderr_devnull(self):
        self._args['stderr'] = subprocess.DEVNULL

    """ PIPE """

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
        self._args['stdin'] = subprocess.PIPE

    def set_stdout_pipe(self):
        self._args['stdout'] = subprocess.PIPE

    def set_stderr_pipe(self):
        self._args['stderr'] = subprocess.PIPE

    """ STDERR """

    def set_stderr_out(self):
        """ Routes stderr to stdout """
        self._args['stderr'] = subprocess.STDOUT
