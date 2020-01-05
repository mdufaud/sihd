#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import sys
import time

import test_utils
import sihd

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

def test_service(cmd):
    interactor = sihd.Interactors.sys.CmdInteractor()
    interactor.set_conf({
        "cmd": cmd,
        "devnull": "stdout"
    })
    assert(interactor.setup())
    assert(interactor.start())
    time.sleep(0.1)
    #thread frequency base is 50hz -> 0.1 sec = ~5 iter
    assert(interactor._thread._iter >= 4)
    assert(interactor.stop())

def test_cmd(cmd):
    interactor = sihd.Interactors.sys.CmdInteractor()
    assert(interactor.exe(cmd))

def test_err(cmd):
    interactor = sihd.Interactors.sys.CmdInteractor()
    assert(interactor.exe(cmd) is False)

def test_pipe(cmd1, cmd2):
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
    assert(proc1)
    # Do not communicate() and end proc1 before piping
    itrc2.set_stdin(proc1.stdout)
    proc2 = itrc2.execute()
    assert(proc2)
    out, errs = itrc2.communicate()
    itrc1.end_process()
    assert(errs is None)
    assert(out.decode('ascii') == "12\n")
    assert(proc1.returncode == 0)
    assert(proc2.returncode == 0)

def test_err_str(cmd):
    interactor = sihd.Interactors.sys.CmdInteractor()
    interactor.set_stderr_pipe()
    interactor.execute(cmd)
    out, errs = interactor.communicate()
    assert(errs is not None)
    assert(errs.decode('ascii').find('no_folder') > 0)
    assert(len(errs.decode('ascii')) > 10)

def test_err_pipe(cmd1, cmd2):
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
    assert(proc1)
    itrc2.set_stdin(proc1.stdout)
    proc2 = itrc2.execute()
    assert(proc2 is None)
    itrc1.end_process()

def test_communication(cmd, data):
    itrc = sihd.Interactors.sys.CmdInteractor()
    itrc.set_cmd(cmd)
    itrc.set_stdin_pipe()
    itrc.set_stdout_pipe()
    itrc.set_stderr_out()
    """ Test should success """
    proc = itrc.execute()
    out, errs = itrc.communicate(timeout=2, input=data.encode())
    assert(errs is None)
    assert(out.decode() == "{}\n".format(len(data)))
    """ Test should fail as no input comes """
    proc = itrc.execute()
    out, errs = itrc.communicate(timeout=1)
    assert(errs is None)
    assert(out.decode() == "0\n")

if __name__ == '__main__':
    logger.info("Starting test")
    ls = "ls -l"
    test_service(ls)
    echo = "echo Hello World"
    test_cmd(echo)
    wc = "wc -c"
    test_pipe(echo, wc)
    err = "/no_file"
    err2 = "ls no_folder"
    test_err(err)
    test_err_str(err2)
    test_err_pipe(ls, err)
    test_communication(wc, "Hello World")
    logger.info("Test ending")
