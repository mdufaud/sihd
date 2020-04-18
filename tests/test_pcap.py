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

class PcapTestHandler(IHandler):

    def __init__(self, reader, app=None, name="PcapTestHandler"):
        super(PcapTestHandler, self).__init__(app=app, name=name)
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

class TestPcap(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    @staticmethod
    def make_pcap(path, lst):
        writer = PcapWriter(endian="little")
        for el in lst:
            writer.add_data(el)
        writer.write_pcap(path)

    def test_pcap_reader(self):
        pcap_path = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test_simple.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)
        reader = sihd.Readers.PcapReader()
        #reader.set_conf("path", pcap_path)
        self.assertTrue(reader.setup())
        reader.path.write(pcap_path)
        handler = PcapTestHandler(reader)
        logger.info("###### Setup done. Starting ######")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        self.assertTrue(len(handler.received) > 0)
        for i, el in enumerate(handler.received):
            self.assertEqual(el, lst[i])

    def test_pcap_saver(self):
        pcap_path = os.path.join(os.path.dirname(__file__), "resources", "Pcap", "test.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)

        saver = sihd.Handlers.PcapHandler()
        reader = sihd.Readers.PcapReader()
        handler = PcapTestHandler(reader)

        reader.set_conf("service_type", "process")
        reader.set_conf("path", pcap_path)

        """
        reader.set_reader_saving(True)
        reader.set_channel_saving('packet')
        """
        
        self.assertTrue(saver.setup())
        self.assertTrue(reader.setup())

        reader.packet.add_observer(saver.save)
        saver.activate.write(True)

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
