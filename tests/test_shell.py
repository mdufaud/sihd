#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import unittest

import utils
import sihd
logger = sihd.set_log()

from sihd.Handlers.AHandler import AHandler

class TestShell(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    def test_complex_pipe_interactor(self):
        itrc = sihd.Interactors.sys.PipeInteractor()
        itrc.set_conf({
            "cmd1": "echo 'Hello World\nTest'",
            "cmd2": "grep 'e'",
            "cmd3": "awk '{print $1}'",
            "cmd4": "sort -r",
        })
        self.assertTrue(itrc.setup())
        self.assertTrue(itrc.interact())
        code, out, err, to = itrc.result.read()
        self.assertEqual(err, None)
        self.assertEqual(out.decode(), "Test\nHello\n")
        self.assertEqual(code, 0)

    def test_pipe_interactor(self):
        itrc = sihd.Interactors.sys.PipeInteractor()
        itrc.set_conf({
            "cmd1": "echo Hello World",
            "cmd2": "wc -c"
        })
        self.assertTrue(itrc.setup())
        self.assertTrue(itrc.interact())
        code, out, err, to = itrc.result.read()
        self.assertEqual(err, None)
        self.assertEqual(out.decode(), "12\n")
        self.assertEqual(code, 0)

    def test_interactor_service(self):
        cmd = "ls -l"
        interactor = sihd.Interactors.sys.ShellInteractor()
        interactor.set_conf({
            "cmd": cmd,
            "devnull": "stdout",
            "runnable_type": "thread",
        })
        self.assertTrue(interactor.setup())
        self.assertTrue(interactor.start())
        time.sleep(0.1)
        self.assertTrue(interactor.stop())
        self.assertEqual(interactor.result.read()[0], 0)

    def test_cmd(self):
        cmd = "echo Hello World"
        itrc = sihd.Interactors.sys.ShellInteractor()
        self.assertTrue(itrc.exe(cmd))

    def test_err(self):
        cmd = "/no_file"
        interactor = sihd.Interactors.sys.ShellInteractor()
        self.assertTrue(interactor.exe(cmd) is False)

    def test_channel(self):
        inpt = "Hello World"
        cmd = "wc -c"
        # Second cmd
        itrc = sihd.Interactors.sys.ShellInteractor()
        itrc.set_conf({
            "cmd": cmd,
            "pipe": "stdout;stderr",
            "runnable_type": "thread"
        })
        itrc.setup()
        #Set stdin input
        itrc.stdin.write(inpt)
        itrc.start()
        time.sleep(0.5)
        itrc.pause()
        code, out, err, to = itrc.result.read()
        print(code, out, err, to)
        self.assertEqual(err, None)
        self.assertEqual(out.decode(), "11\n")
        self.assertEqual(code, 0)
        itrc.stdin.write(False)
        itrc.resume()
        #Reset input - Should be an error
        time.sleep(0.5)
        itrc.stop()
        code, out, err, timedout = itrc.result.read()
        print(code, out, err, timedout)
        self.assertTrue(timedout is False)
        self.assertTrue(err is not None)
        self.assertEqual(out.decode(), "0\n")
        self.assertNotEqual(code, 0)

    def test_pipe(self):
        cmd1 = "echo Hello World"
        cmd2 = "wc -c"
        # First cmd - Piping result
        itrc1 = sihd.Interactors.sys.ShellInteractor()
        itrc1.set_conf({
            "cmd": cmd1,
            "pipe": "stdout"
        })
        itrc1.setup()
        # Second cmd
        itrc2 = sihd.Interactors.sys.ShellInteractor()
        itrc2.set_conf({
            "cmd": cmd2,
            "pipe": "stdout",
        })
        itrc2.setup()
        # Execution
        proc1 = itrc1.execute()
        self.assertTrue(proc1 is not None)
        # Do not communicate() proc1 and set 1'stdout as 2'stdin
        itrc2.set_stdin(proc1.stdout)
        # Execute proc2
        proc2 = itrc2.execute()
        self.assertTrue(proc2)
        # Comm proc2
        out, errs, to = itrc2.communicate()
        # End proc1
        itrc1.end_process()
        self.assertTrue(errs is None)
        self.assertEqual(out.decode('ascii'), "12\n")
        self.assertTrue(proc1.returncode == 0)
        self.assertTrue(proc2.returncode == 0)

    def test_err_str(self):
        #Goal is to test communication when error
        args = ["ls", "no_folder"]
        interactor = sihd.Interactors.sys.ShellInteractor()
        interactor.set_stderr_pipe()
        interactor.execute(args)
        out, errs, to = interactor.communicate()
        self.assertTrue(errs is not None)
        self.assertTrue(errs.decode('ascii').find('no_folder') > 0)
        self.assertTrue(len(errs.decode('ascii')) > 10)

    def test_err_pipe(self):
        #Goal is to test error when piping proc fails
        cmd1 = "ls -l"
        cmd2 = "/no_file"
        # First cmd - Piping result
        itrc1 = sihd.Interactors.sys.ShellInteractor()
        itrc1.set_conf({
            "cmd": cmd1,
            "pipe": "stdout"
        })
        itrc1.setup()
        # Second cmd
        itrc2 = sihd.Interactors.sys.ShellInteractor()
        itrc2.set_conf({
            "cmd": cmd2,
        })
        itrc2.setup()
        # Execution
        proc1 = itrc1.execute()
        self.assertTrue(proc1)
        itrc2.set_stdin(proc1.stdout)
        proc2 = itrc2.execute()
        self.assertTrue(proc2 is None)
        itrc1.end_process()

    def test_communication(self):
        cmd = "wc -c"
        data = "Hello World"
        itrc = sihd.Interactors.sys.ShellInteractor()
        itrc.set_cmd(cmd)
        itrc.set_stdin_pipe()
        itrc.set_stdout_pipe()
        itrc.set_stderr_out()
        # Test should success
        proc = itrc.execute()
        out, errs, to = itrc.communicate(timeout=2, input=data.encode())
        self.assertTrue(errs is None)
        self.assertTrue(out.decode() == "{}\n".format(len(data)))
        # Test should fail as no input comes
        proc = itrc.execute()
        out, errs, to = itrc.communicate(timeout=1)
        self.assertTrue(errs is None)
        self.assertTrue(out.decode() == "0\n")

if __name__ == '__main__':
    unittest.main()
