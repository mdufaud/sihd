#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time

import test_utils
import sihd

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

import socket
import unittest

from sihd.srcs.Handlers.IHandler import IHandler

from sihd.srcs.Tools.pcap import PcapWriter

class StdinHandler(IHandler):

    def __init__(self, app=None, name="StdinHandler"):
        super(StdinHandler, self).__init__(app=app, name=name)
        self._step = 0

    def handle(self, reader, line):
        if line is None:
            return None
        line = line.decode('ascii')
        if line == "":
            print()
            logger.info("Client has stopped input")
            return
        line = line.strip()
        if line == "":
            return
        print("Received: '{}'".format(line))
        step = self._step
        if step == 0:
            reader.set_question("Great - Type 5 now: ")
            self._step = 1
        elif step == 1 and line == '5':
            reader.set_question("Thanks ! Type q to quit: ")
            self._step = 2
        elif step == 2 and line == 'q':
            reader.stop()
        return True

class PcapHandler(IHandler):

    def __init__(self, app=None, name="PcapHandler"):
        super(PcapHandler, self).__init__(app=app, name=name)
        self.received = []
        self.print_hdr = False

    def handle(self, reader, pkt):
        if self.print_hdr is False:
            print(reader.get_pcap_header())
            self.print_hdr = True
        s = "data: " + str(pkt[:10])
        info = reader.get_pkt_info()
        if info.cap_len > 10:
            s += "..."
        print(info, s)
        assert(len(pkt) == info.cap_len)
        self.received.append(pkt.decode())

class TestReader(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_stdin_reader(self):
        reader = sihd.Readers.StdinReader()
        handler = StdinHandler()
        reader.add_observer(handler)
        reader.set_conf("question", "How are you ? ")
        reader.setup()
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        try:
            while reader.is_active():
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nKeyboard Interruption")
            pass
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())

    @staticmethod
    def make_pcap(path, lst):
        writer = PcapWriter(endian="little")
        for el in lst:
            writer.add_pkt(el)
        writer.write_pcap(path)

    def test_pcap_reader(self):
        pcap_path = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test_simple.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)
        reader = sihd.Readers.PcapReader()
        reader.set_conf("path", pcap_path)
        self.assertTrue(reader.setup())
        handler = PcapHandler()
        reader.add_observer(handler)
        logger.info("Setup done. Starting")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        for i, el in enumerate(handler.received):
            self.assertEqual(el, lst[i])

    def test_dump(self):
        pcap_path = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)
        reader = sihd.Readers.PcapReader()
        handler = PcapHandler()
        reader.add_observer(handler)
        reader.set_conf("path", pcap_path)
        reader.setup()
        reader.save_data(True)
        logger.info("Setup done. Starting")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        logger.info("Dumping")
        """ We dump then load from dump and check if saved data are the same """
        saved = reader.get_data_saved()
        reader.set_dump_magic("--TEST--")
        path = os.path.join(os.path.dirname(__file__), "resources", "Dump", "test.dump")
        reader.dump_to(path)
        reader.clear_data_saved()
        reader.load_from(path)
        loaded = reader.get_data_saved()
        self.assertEqual(saved, loaded)
        """ Testing a dump on the handler too """
        path = os.path.join(os.path.dirname(__file__), "resources", "Dump", "test_handler.dump")
        handler.dump_to(path)
        rcv = handler.received
        handler.load_from(path)
        rcv2 = handler.received
        self.assertEqual(rcv, rcv2)
        handler.received = []
        logger.info("Starting again after dumps")
        """ We then test the reader to see if it still works from being pickled loaded """
        pcap_path2 = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test2.pcap")
        lst = ['dont', 'forget', 'your', 'nems']
        self.make_pcap(pcap_path2, lst)
        reader.set_source(pcap_path2)
        reader.clear_data_saved()
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        saved = reader.get_data_saved()
        self.assertEqual(saved, [(el.encode('utf-8'),) for el in lst])
        for i, el in enumerate(handler.received):
            self.assertEqual(el, lst[i])

if __name__ == '__main__':
    unittest.main(verbosity=2)
