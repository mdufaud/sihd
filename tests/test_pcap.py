#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import socket
import unittest

import utils
import sihd
logger = sihd.log.setup('info')

from sihd.handlers.AHandler import AHandler

from sihd.readers.file import PcapReader
from sihd.handlers.sys import PcapHandler
from sihd.handlers.utils import DuplicatorHandler

class PcapTestHandler(AHandler):

    def __init__(self, test, app=None, name="PcapTestHandler"):
        super(PcapTestHandler, self).__init__(app=app, name=name)
        self.received = []
        self.add_channel_input("hdr", type='queue', simple=True)
        self.add_channel_input("infos", type='queue')
        self.add_channel_input("pkt", type='queue')
        self.test = test
        self.once = False

    def handle_service_pcapreader(self, service):
        # Fill our channels
        service.pcap_header.add_observer(self.hdr)
        service.packet_info.add_observer(self.infos)
        # Callback when packet is read on us
        service.packet.add_observer(self)

    def on_notify(self, channel):
        if not self.once:
            print("Pcap Header:", self.hdr.read())
            self.once = True
        self.last_pkt_info = self.infos.read()
        pkt = channel.read()
        s = "data: " + str(pkt[:10])
        info = self.last_pkt_info
        if info.cap_len > 10:
            s += "..."
        print(info, s)
        self.test.assertEqual(len(pkt), info.cap_len)
        self.received.append(pkt.decode())

class TestPcap(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    @staticmethod
    def make_pcap(path, lst):
        writer = sihd.utils.pcap.PcapWriter(endian="little")
        for el in lst:
            writer.add_data(el)
        writer.write_pcap(path)

    def test_pcap_reader(self):
        pcap_path = os.path.join(os.path.dirname(__file__), "output", "test_simple.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)
        reader = PcapReader()
        self.assertTrue(reader.setup())
        reader.path.write(pcap_path)
        handler = PcapTestHandler(self)
        handler.configuration.set('runnable_type', 'none')
        handler.setup()
        handler.handle_service(reader)
        logger.info("###### Setup done. Starting ######")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(0.5)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        self.assertTrue(len(handler.received) > 0)
        for i, el in enumerate(handler.received):
            self.assertEqual(el, lst[i])

    @unittest.skipIf(utils.is_multiprocessing() is False, "No support for multiprocess")
    def test_pcap_saver(self):
        pcap_path = os.path.join(os.path.dirname(__file__),
                                "output", "test.pcap")
        lst = ['hello', 'world', 'are', 'you', 'alive']
        self.make_pcap(pcap_path, lst)

        saver = PcapHandler()
        saver.set_channel_conf('save', type='queue')
        reader = PcapReader()
        reader.set_channel_conf("packet", type='queue')
        handler = PcapTestHandler(self)
        duplicator = DuplicatorHandler()
        saver.configuration.load({
            "save_raw": 1,
            "save_type": 'list',
            "activate": 0,
            "endianness": "big",
            "runnable_type": "process"
        })
        reader.configuration.set("runnable_type", "process")
        reader.configuration.set("path", pcap_path)

        self.assertTrue(saver.setup())
        self.assertTrue(reader.setup())
        self.assertTrue(handler.setup())
        self.assertTrue(duplicator.setup())

        handler.link('hdr', reader.pcap_header)
        handler.link('infos', reader.packet_info)

        duplicator.link("input", reader.packet)
        duplicator.duplicate_to(saver.save)
        duplicator.duplicate_to(handler.pkt)

        saver.activate.write(True)

        logger.info("###### Setup done. Starting ######")
        self.assertTrue(duplicator.start())
        self.assertTrue(handler.start())
        self.assertTrue(saver.start())
        self.assertTrue(reader.start())
        time.sleep(0.5)
        self.assertTrue(reader.stop())
        self.assertTrue(saver.pause())
        self.assertTrue(handler.stop())

        logger.info("Testing saved data")
        data = saver.saved.get_data()
        data_saved = [el.decode() for el in data]
        self.assertEqual(data_saved, lst)

        path = os.path.join(os.path.dirname(__file__),
                            "output", "pcap_test.dump")
        logger.info("Asking to dump to {}".format(path))
        saver.dump_path.write(path)
        self.assertTrue(saver.resume())
        time.sleep(0.3)
        self.assertTrue(saver.stop())

        logger.info("Reading dump with another handler")
        saver = PcapHandler('PcapHandler2')
        saver.configuration.load({
            "save_raw": 1,
            "save_type": 'list',
            "activate": 1,
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
