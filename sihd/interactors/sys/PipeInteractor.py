#!/usr/bin/python
# coding: utf-8

#
# System
#
subprocess = None
shlex = None

from sihd.interactors.AInteractor import AInteractor
from .ShellInteractor import ShellInteractor

class PipeInteractor(AInteractor):

    def __init__(self, name="PipeInteractor", **kwargs):
        super().__init__(name=name)
        global subprocess
        if subprocess is None:
            import subprocess
        global shlex
        if shlex is None:
            import shlex
        self.__interactor_lst = []
        self.__idx = 0
        self.add_channel_input("stdin")

    #
    # Configuration
    #

    def on_setup(self, conf):
        ret = super().on_setup(conf)
        i = 1
        while True:
            cmd = conf.get("cmd" + str(i), dynamic=True)
            if cmd is not None:
                self.add_pipe(cmd)
            else:
                break
            i += 1
        return True

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

    def add_pipe(self, cmd):
        self.__idx += 1
        self.log_debug("Adding pipe command {}: {}".format(self.__idx, cmd))
        interactor = ShellInteractor(name="cmd_" + str(self.__idx))
        interactor.set_cmd(cmd)
        interactor.set_stdout_pipe()
        interactor.set_stderr_pipe()
        self.__interactor_lst.append(interactor)

    def set_input(self, data):
        lst = self.__interactor_lst
        if lst:
            interactor = lst[0]
            interactor.set_input(data)
        return len(lst) >= 1

    def __end_proc(self, idx=-1, kill=False):
        idx = len(lst) if idx == -1 else idx
        lst = self.__interactor_lst
        #Idx is at end of list so -1 and we dont end last proc -> -2
        idx = idx - 2
        while idx >= 0:
            interactor = lst[idx]
            interactor.end_process(kill=kill)
            idx -= 1

    def on_new_interaction(self, action):
        return action

    def on_interaction(self, cmd, *args, **kwargs):
        ret = False
        child = None
        idx = 0
        last_itrc = None
        last_child = None
        for interactor in self.__interactor_lst:
            if last_child is not None:
                interactor.set_stdin(last_child.stdout)
            child = interactor.execute(cmd)
            if child is None:
                self.__end_proc(idx)
                return False
            last_itrc = interactor
            last_child = child
            idx += 1
        if last_itrc:
            out, err, to = last_itrc.communicate()
            self.set_result((child.returncode, out, err, to))
            self.__end_proc(idx)
        return child.returncode == 0

    def on_stop(self):
        super().on_stop()
        self.__interactor_lst = []
