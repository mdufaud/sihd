#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import sys
import time

import sihd

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

import unittest

from sihd.srcs.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self.out = None
        self.err = None

    def handle(self, reader, data):
        print("Received: ", data)
        self.out = data.decode()
        return True

    def on_error(self, reader, error):
        print("Error rcv: ", error)
        self.err = error.decode()
        return True

class TestCmd(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def do_cmd_reader(self, cmd, out=None, err=None):
        reader = sihd.Readers.CmdReader()
        reader.set_conf({
            "cmd": cmd
        })
        h = TestHandler()
        reader.add_observer(h)
        reader.setup()
        self.assertTrue(reader.start())
        try:
            time.sleep(0.3)
        except KeyboardInterrupt:
            print("\nKeyboard Interruption")
            pass
        self.assertTrue(reader.stop())
        if out:
            self.assertTrue(h.out == out)
        if err:
            self.assertTrue(h.err.find(err) != -1)

    def test_reader_service(self):
        self.do_cmd_reader("echo Hello World", out="Hello World\n")
        self.do_cmd_reader("ls /smth", err="/smth")

    def do_pipe_cmd_reader(self, cmd1, cmd2, out=None, err=None):
        reader1 = sihd.Readers.CmdReader(name="FirstCmd")
        reader1.set_conf("cmd", cmd1)
        reader2 = sihd.Readers.CmdReader(name="SecondCmd")
        reader2.set_conf("cmd", cmd2)
        reader1.setup()
        reader2.setup()
        reader2.configure_pipe_cmd_reader(reader1)
        h = TestHandler()
        reader2.add_observer(h)
        self.assertTrue(reader1.start())
        self.assertTrue(reader2.start())
        try:
            time.sleep(0.5)
        except KeyboardInterrupt:
            print("\nKeyboard Interruption")
            pass
        self.assertTrue(reader2.stop())
        self.assertTrue(reader1.stop())
        if out:
            self.assertTrue(h.out == out)
        if err:
            self.assertTrue(h.err.find(err) != -1)

    def test_reader_pipe_service(self):
        self.do_pipe_cmd_reader("echo Hello World", "wc -c", out="12\n")

    def test_interactor_service(self):
        cmd = "ls -l"
        interactor = sihd.Interactors.sys.CmdInteractor()
        interactor.set_conf({
            "cmd": cmd,
            "devnull": "stdout"
        })
        self.assertTrue(interactor.setup())
        self.assertTrue(interactor.start())
        time.sleep(0.1)
        #thread frequency base is 50hz -> 0.1 sec = ~5 iter
        self.assertTrue(interactor._thread._iter >= 4)
        self.assertTrue(interactor.stop())

    def test_cmd(self):
        cmd = "echo Hello World"
        interactor = sihd.Interactors.sys.CmdInteractor()
        self.assertTrue(interactor.exe(cmd))

    def test_err(self):
        cmd = "/no_file"
        interactor = sihd.Interactors.sys.CmdInteractor()
        self.assertTrue(interactor.exe(cmd) is False)

    def test_pipe(self):
        cmd1 = "echo Hello World"
        cmd2 = "wc -c"
        """ First cmd - Piping result """
        itrc1 = sihd.Interactors.sys.CmdInteractor()
        itrc1.set_conf({
            "cmd": cmd1,
            "pipe": "stdout"
        })
        itrc1.setup()
        """ Second cmd """
        itrc2 = sihd.Interactors.sys.CmdInteractor()
        itrc2.set_conf({
            "cmd": cmd2,
            "pipe": "stdout",
        })
        itrc2.setup()
        """ Execution """
        proc1 = itrc1.execute()
        self.assertTrue(proc1)
        # Do not communicate() and end proc1 before piping
        itrc2.set_stdin(proc1.stdout)
        proc2 = itrc2.execute()
        self.assertTrue(proc2)
        out, errs = itrc2.communicate()
        itrc1.end_process()
        self.assertTrue(errs is None)
        self.assertTrue(out.decode('ascii') == "12\n")
        self.assertTrue(proc1.returncode == 0)
        self.assertTrue(proc2.returncode == 0)

    def test_err_str(self):
        cmd = "ls no_folder"
        interactor = sihd.Interactors.sys.CmdInteractor()
        interactor.set_stderr_pipe()
        interactor.execute(cmd)
        out, errs = interactor.communicate()
        self.assertTrue(errs is not None)
        self.assertTrue(errs.decode('ascii').find('no_folder') > 0)
        self.assertTrue(len(errs.decode('ascii')) > 10)

    def test_err_pipe(self):
        cmd1 = "ls -l"
        cmd2 = "/no_file"
        """ First cmd - Piping result """
        itrc1 = sihd.Interactors.sys.CmdInteractor()
        itrc1.set_conf({
            "cmd": cmd1,
            "pipe": "stdout"
        })
        itrc1.setup()
        """ Second cmd """
        itrc2 = sihd.Interactors.sys.CmdInteractor()
        itrc2.set_conf({
            "cmd": cmd2,

        })
        itrc2.setup()
        """ Execution """
        proc1 = itrc1.execute()
        self.assertTrue(proc1)
        itrc2.set_stdin(proc1.stdout)
        proc2 = itrc2.execute()
        self.assertTrue(proc2 is None)
        itrc1.end_process()

    def test_communication(self):
        cmd = "wc -c"
        data = "Hello World"
        itrc = sihd.Interactors.sys.CmdInteractor()
        itrc.set_cmd(cmd)
        itrc.set_stdin_pipe()
        itrc.set_stdout_pipe()
        itrc.set_stderr_out()
        """ Test should success """
        proc = itrc.execute()
        out, errs = itrc.communicate(timeout=2, input=data.encode())
        self.assertTrue(errs is None)
        self.assertTrue(out.decode() == "{}\n".format(len(data)))
        """ Test should fail as no input comes """
        proc = itrc.execute()
        out, errs = itrc.communicate(timeout=1)
        self.assertTrue(errs is None)
        self.assertTrue(out.decode() == "0\n")


if __name__ == '__main__':
    unittest.main()
