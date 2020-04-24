#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import unittest

import sihd
logger = sihd.set_log()

from sihd.Handlers.IHandler import IHandler

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
        self.assertEqual(itrc.stderr.read(), None)
        self.assertEqual(itrc.stdout.read().decode(), "Test\nHello\n")
        self.assertEqual(itrc.returncode.read(), 0)

    def test_pipe_interactor(self):
        itrc = sihd.Interactors.sys.PipeInteractor()
        itrc.set_conf({
            "cmd1": "echo Hello World",
            "cmd2": "wc -c"
        })
        self.assertTrue(itrc.setup())
        self.assertTrue(itrc.interact())
        self.assertEqual(itrc.stderr.read(), None)
        self.assertEqual(itrc.stdout.read().decode(), "12\n")
        self.assertEqual(itrc.returncode.read(), 0)

    def test_interactor_service(self):
        cmd = "ls -l"
        interactor = sihd.Interactors.sys.ShellInteractor()
        interactor.set_conf({
            "cmd": cmd,
            "devnull": "stdout",
            "service_type": "thread",
        })
        self.assertTrue(interactor.setup())
        self.assertTrue(interactor.start())
        time.sleep(0.1)
        self.assertTrue(interactor.stop())
        self.assertEqual(interactor.returncode.read(), 0)

    def test_cmd(self):
        cmd = "echo Hello World"
        itrc = sihd.Interactors.sys.ShellInteractor()
        self.assertTrue(itrc.exe(cmd))

    def test_err(self):
        cmd = "/no_file"
        interactor = sihd.Interactors.sys.ShellInteractor()
        self.assertTrue(interactor.exe(cmd) is False)

    def test_channel(self):
        res = "Hello World"
        cmd = "wc -c"
        # Second cmd
        itrc = sihd.Interactors.sys.ShellInteractor()
        itrc.set_conf({
            "cmd": cmd,
            "pipe": "stdout;stderr",
            "service_type": "thread"
        })
        itrc.setup()
        #Set stdin input
        itrc.stdin.write(res)
        itrc.start()
        time.sleep(0.5)
        itrc.pause()
        self.assertEqual(itrc.stderr.read(), None)
        self.assertEqual(itrc.stdout.read().decode(), "11\n")
        self.assertEqual(itrc.returncode.read(), 0)
        itrc.resume()
        #Reset input - Should be an error
        itrc.stdin.write(False)
        time.sleep(0.5)
        itrc.stop()
        self.assertTrue(itrc.stderr.read() is not None)
        self.assertEqual(itrc.stdout.read().decode(), "0\n")
        self.assertEqual(itrc.returncode.read(), 1)

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
        out, errs = itrc2.communicate()
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
        out, errs = interactor.communicate()
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
        out, errs = itrc.communicate(timeout=2, input=data.encode())
        self.assertTrue(errs is None)
        self.assertTrue(out.decode() == "{}\n".format(len(data)))
        # Test should fail as no input comes
        proc = itrc.execute()
        out, errs = itrc.communicate(timeout=1)
        self.assertTrue(errs is None)
        self.assertTrue(out.decode() == "0\n")

if __name__ == '__main__':
    unittest.main()
