#!/usr/bin/python
# coding: utf-8

""" System """
import sys
import re
import cmd
import time

from sihd.gui.AGui import AGui
from sihd import core

class ACmdGui(cmd.Cmd, AGui):

    prompt = '?> '
    intro = None

    def __init__(self, name="ACmdGui", app=None):
        cmd.Cmd.__init__(self)
        AGui.__init__(self, app=app, name=name)
        self.__completion = {}
        self.configuration.add_defaults({
            "prompt": "$> ",
            "intro": "",
        })
        self.__services_fun = ["start", "stop", "resume", "pause", "setup"]
        self.set_completion("service_any", self.__services_fun)

    #
    # AConfigurable
    #

    def on_setup(self, conf):
        ret = super().on_setup(conf)
        self.set_channel_notification(True)
        prompt = conf.get("prompt")
        intro = conf.get("intro")
        if prompt:
            self.set_prompt(prompt)
        if intro:
            self.set_intro(intro)
        return ret

    @staticmethod
    def set_prompt(prompt):
        ACmdGui.prompt = prompt

    @staticmethod
    def set_intro(intro):
        ACmdGui.intro = intro

    #
    # core
    #

    def emptyline(self):
        pass

    def postcmd(self, stop, line):
        if self.is_active() is False:
            stop = True
        return stop

    #
    # Completion
    #

    @staticmethod
    def __replace_sub(string, char):
        """ From "reeeeegex" -> returns "regex" """
        pattern = char + '{2,}'
        string = re.sub(pattern, char, string)
        return string

    @staticmethod
    def __get_current_path_to_completion(line, beg_idx):
        """ From "cmd   arg    som" -> returns "cmd_arg" """
        path = line[:beg_idx].strip().replace(' ', '_')
        path = path.strip("_")
        path = ACmdGui.__replace_sub(path, "_")
        return path

    @staticmethod
    def __get_parent_path(line):
        """ From "parent_child" -> returns "parent" """
        path = None
        idx = line.rfind("_")
        if idx != -1:
            path = line[:idx]
        return path

    def completedefault(self, text, line, beg_idx, end_idx):
        """
            With completion = {
                "cmd": ["something", "hey"],
                "cmd_any": ["another"]
                "cmd_yay_any": ['magics'],
            }
            From "cmd " -> returns ["something", "hey"]
            From "cmd s" -> returns ["something"]
            From "cmd arg a" -> returns ["another"]
            From "cmd arg2 " -> returns ["another"]
            From "cmd yay " -> returns ["magics"]
        """
        ret = []
        self.reload_completions(text, line, beg_idx, end_idx)
        completion = self.__completion
        path = self.__get_current_path_to_completion(line, beg_idx)
        line = self.__replace_sub(line, ' ')
        if path:
            parent = self.__get_parent_path(path)
            complete_exact = completion.get(path, None)
            if complete_exact is not None:
                ret.extend([l for l in complete_exact if l.startswith(text)])
            if parent:
                complete_any = completion.get("{}_any".format(parent), None)
                if complete_any is not None:
                    ret.extend([l for l in complete_any if l.startswith(text)])
        return ret

    def __set_services_completion(self):
        """ Get services names from app """
        app = self.get_app()
        if not app:
            return
        status = app.get_services_status()
        lst = []
        for service_type, services_status in status.items():
            for name, status in services_status.items():
                lst.append(name)
        if lst:
            self.set_completion("service", lst)

    def set_completion(self, cmd, arg):
        if isinstance(arg, (list, set)):
            self.__completion[cmd] = arg
        else:
            self.__completion[cmd] = [arg]

    def reload_completions(self, text, line, beg_idx, end_idx):
        """ Called before completion to update """
        if line.startswith("service"):
            self.__set_services_completion()

    #
    # Basic commands
    #

    def do_status(self, args):
        """ Get application's service status """
        app = self.get_app()
        if not app:
            print("No application for services")
            return
        status = app.get_services_status()
        for service_type, services_status in status.items():
            print("{}: ".format(service_type))
            for name, status in services_status.items():
                print("\t{}: {}".format(name, status))

    def help_status(self):
        print("Shows application's status services")

    def do_service(self, args):
        """ Can start stop resume and pause app's services """
        app = self.get_app()
        if not app:
            print("No application for services")
            return
        split = args.split()
        if len(split) != 2:
            self.help_service()
            return
        name = split[0]
        cmd = split[1]
        if cmd not in self.__services_fun:
            print("No such state: {}".format(cmd))
            self.help_service()
            return
        service = app.get_service(name)
        if not service:
            print("No service named: {}".format(args))
            return
        try:
            attr = getattr(service, cmd)
        except AttributeError:
            print("No method {} for service {}".format(cmd, service))
            return
        attr()

    def help_service(self):
        print("service [service_name] [start|stop|pause|resume|setup]")

    def do_exit(self, args):
        """ Exit cmdloop """
        self.log_info("Exiting {} command interpreter".format(self.get_name()))
        return True

    def help_exit(self):
        print("Exit the application")

    #
    # Handle ctrl + d
    #

    def do_EOF(self, args):
        print()
        return self.do_exit(args)

    help_EOF = help_exit

    def postloop(self):
        self.stop()

    #
    # Children implementations
    #

    def default(self, args):
        """ When no do_* is found """
        pass

    #
    # AGui
    #

    def loop(self, **kwargs):
        time.sleep(0.1)
        self.poll_channels_input()
        self.cmdloop()

    #
    # SihdObject
    #

    def on_pause(self):
        print()
        super().on_pause()
