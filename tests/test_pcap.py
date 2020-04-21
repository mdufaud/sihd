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

try:
    import multiprocessing
    if multiprocessing is not None:
        #checks for /dev/shm
        val = multiprocessing.Value('i', 0)
except (ImportError, FileNotFoundError):
    multiprocessing = None

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
        pcap_path = os.path.join(os.path.dirname(__file__),
                                "resources", "Pcap", "test.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)

        saver = sihd.Handlers.PcapHandler()
        reader = sihd.Readers.PcapReader()
        handler = PcapTestHandler(reader)

        saver.set_conf({
            "save_raw": True,
            "save_type": 'list',
            "activate": False,
            "endianness": "big",
            "service_type": "process"
        })
        reader.set_conf("service_type", "process")
        reader.set_conf("path", pcap_path)

        self.assertTrue(saver.setup())
        self.assertTrue(reader.setup())

        reader.packet.add_observer(saver.save)
        saver.activate.write(True)

        logger.info("###### Setup done. Starting ######")
        self.assertTrue(handler.start())
        self.assertTrue(saver.start())
        self.assertTrue(reader.start())
        time.sleep(1)
        self.assertTrue(reader.stop())
        self.assertTrue(saver.pause())
        self.assertTrue(handler.stop())

        logger.info("Testing saved data")
        data = saver.saved.get_data()
        data_saved = [el.decode() for el in data]
        self.assertEqual(data_saved, lst)

        path = os.path.join(os.path.dirname(__file__),
                            "outputs", "pcap_test.dump")
        logger.info("Dumping to {}".format(path))
        saver.dump_path.write(path)
        self.assertTrue(saver.resume())
        time.sleep(0.01)
        self.assertTrue(saver.stop())
        
        logger.info("Reading dump with another handler")
        saver = sihd.Handlers.PcapHandler()
        saver.set_conf({
            "save_raw": True,
            "save_type": 'list',
            "activate": True,
            "endianness": "big",
        })
        self.assertTrue(saver.setup())
        self.assertTrue(saver.load_from(path))
        logger.info("Testing reload")
        loaded = saver.saved.get_data()
        data_saved = [el.decode() for el in loaded]
        self.assertEqual(data_saved, lst)

if __name__ == '__main__':
    unittest.main(verbosity=2)
