#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time
import test_utils
import socket
import unittest

import sihd
logger = sihd.set_log('debug')

from sihd.Handlers.IHandler import IHandler

from sihd.Tools.pcap import PcapWriter

class StdinHandler(IHandler):

    def __init__(self, reader, app=None, name="StdinHandler"):
        super(StdinHandler, self).__init__(app=app, name=name)
        self._step = 0
        self.add_channel_input("input", type='queue')
        self.reader = reader

    def handle(self, channel):
        line = channel.read()
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
            self.reader.question.write("Great - Type 5 now: ")
            self._step = 1
        elif step == 1 and line == '5':
            self.reader.question.write("Thanks ! Type q to quit: ")
            self._step = 2
        elif step == 2 and line == 'q':
            self.reader.stop()
        return True

class PcapHandler(IHandler):

    def __init__(self, reader, app=None, name="PcapHandler"):
        super(PcapHandler, self).__init__(app=app, name=name)
        self.received = []
        self.reader = reader
        self.add_channel_input("hdr", type='queue', simple=True)
        self.add_channel_input("infos", type='queue')
        self.add_channel_input("pkt", type='queue')

    def on_setup(self):
        #When channels are done
        self.reader.pcap_header.add_observer(self.hdr)
        self.reader.packet.add_observer(self.pkt)
        self.reader.packet_info.add_observer(self.infos)
        return True

    def handle(self, channel):
        if channel == self.infos:
            self.last_pkt_info = channel.read()
        elif channel == self.pkt:
            pkt = channel.read()
            s = "data: " + str(pkt[:10])
            info = self.last_pkt_info
            if info.cap_len > 10:
                s += "..."
            print(info, s)
            assert(len(pkt) == info.cap_len)
            self.received.append(pkt.decode())
        elif channel == self.hdr:
            print("Pcap Header:", channel.read())

class TestReader(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_stdin_reader(self):
        reader = sihd.Readers.StdinReader()
        handler = StdinHandler(reader)
        self.assertTrue(reader.setup())
        self.assertTrue(handler.setup())
        reader.answer.add_observer(handler.input)
        reader.question.write("How are you ? ")
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
            writer.add_data(el)
        writer.write_pcap(path)

    """
    def test_pcap_reader(self):
        pcap_path = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test_simple.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)
        reader = sihd.Readers.PcapReader()
        #reader.set_conf("path", pcap_path)
        self.assertTrue(reader.setup())
        reader.path.write(pcap_path)
        handler = PcapHandler(reader)
        logger.info("###### Setup done. Starting ######")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        self.assertTrue(len(handler.received) > 0)
        for i, el in enumerate(handler.received):
            self.assertEqual(el, lst[i])
    """

    def test_dump(self):
        pcap_path = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)

        saver = sihd.Handler.PcapSaverHandler()
        reader = sihd.Readers.PcapReader()
        handler = PcapHandler(reader)

        reader.set_conf("service_type", "process")
        reader.set_conf("path", pcap_path)

        """
        reader.set_reader_saving(True)
        reader.set_channel_saving('packet')
        """
        
        self.assertTrue(saver.setup())
        self.assertTrue(reader.setup())

        reader.packet.add_observer(saver.save)

        """
        print(reader)
        print(reader.save_data)
        reader.save_data.write(True)
        print(reader.save_data)
        """

        logger.info("###### Setup done. Starting ######")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())

        """
        logger.info("###### Dumping reader ######")
        #We dump then load from dump and check if saved data are the same
        saved = reader.get_data_saved()
        data_saved = [d.decode() for d in saved]
        self.assertEqual(data_saved, lst)
        reader.set_dump_magic("--TEST--")
        path = os.path.join(os.path.dirname(__file__), "resources", "Dump", "test.dump")
        reader.dump_to(path)
        reader.clear_data_saved()
        reader.load_from(path)
        loaded = reader.get_data_saved()
        print(data_saved, loaded)
        data_loaded = [d.decode() for d in loaded]
        self.assertEqual(data_saved, data_loaded)

        logger.info("###### Dumping handler ######")
        #Testing a dump on the handler too
        path = os.path.join(os.path.dirname(__file__), "resources", "Dump", "test_handler.dump")
        handler.dump_to(path)
        rcv = handler.received
        handler.load_from(path)
        rcv2 = handler.received
        self.assertEqual(rcv, rcv2)
        handler.received = []

        logger.info("###### Reading a new file ######")
        #We then test the reader to see if it still works from being pickled loaded
        pcap_path2 = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test2.pcap")
        lst = ['dont', 'forget', 'your', 'nems']
        self.make_pcap(pcap_path2, lst)
        reader.set_conf('path', pcap_path2)
        self.assertTrue(reader.clear_data_saved())
        logger.info("###### Starting ######")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())

        saved = reader.get_data_saved()
        data_saved = [d.decode() for d in saved]
        self.assertEqual(data_saved, lst)
        for i, el in enumerate(handler.received):
            self.assertEqual(el, lst[i])
        """

if __name__ == '__main__':
    unittest.main(verbosity=2)
