#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import unittest
import json

import utils
import sihd
logger = sihd.log.setup('info')

from sihd.interactors.ip.HttpInteractor import HttpInteractor
from sihd.handlers.AHandler import AHandler

is_internet = sihd.network.ip.is_internet()

class TestHandler(AHandler):

    def __init__(self, name="TestHandler", app=None):
        super().__init__(app=app, name=name)
        self.add_channel_input("input")

    def handle(self, channel):
        data = channel.read()
        if data:
            self.out = data.decode()
            print("Received: ", self.out)
        return True

class TestHttp(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_wrong_url(self):
        logger.info("Testing wrong url")
        url = "http://www.a@E2e2esasfzzzzzzzzzzz.com"
        interactor = HttpInteractor()
        html = interactor.make_request(url).send()
        self.assertTrue(html is None)

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_error_404(self):
        logger.info("Testing error 404")
        url = "http://httpbin.org/status/404"
        interactor = HttpInteractor()
        html = interactor.make_request(url).send()
        self.assertTrue(html is None)

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_get(self):
        logger.info("Testing GET")
        url = "http://httpbin.org/get"
        interactor = HttpInteractor()
        json_data = interactor.make_request(url, query={"get": "cookies"}).send().decode()
        print(json_data)
        self.assertTrue(json_data is not None)
        dic = json.loads(json_data)
        self.assertEqual(dic["url"], "{}?get=cookies".format(url))
        self.assertEqual(dic["args"]['get'], 'cookies')

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_post(self):
        logger.info("Testing POST")
        url = "http://httpbin.org/post"
        interactor = HttpInteractor()
        json_data = interactor.make_request(url, post={'hello': 'world'}).send().decode()
        self.assertTrue(json_data is not None)
        dic = json.loads(json_data)
        self.assertEqual(dic["form"]['hello'], 'world')

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_service(self):
        logger.info("Testing service life cycle")
        url = "https://www.google.com"
        interactor = HttpInteractor()
        interactor.configuration.load({
            "url": url,
            'runnable_type': 'thread'
        })
        self.assertTrue(interactor.setup())
        self.assertTrue(interactor.start())
        time.sleep(0.5)
        req_http = interactor.result.read()
        if req_http is None:
            time.sleep(0.5)
            req_http = interactor.result.read()
        self.assertTrue(interactor.stop())
        self.assertTrue(req_http is not None)

    @unittest.skipIf(utils.is_multiprocessing() is False or is_internet is False,
                     "No support for multiprocess or no internet")
    def test_channels(self):
        logger.info("Testing channels well being")
        url = "https://www.google.com"
        interactor = HttpInteractor()
        interactor.configuration.set('runnable_type', 'process')
        try:
            self.assertTrue(interactor.setup())
        except FileNotFoundError:
            #/dev/shm
            logger.warning("Test cannot continue as your device has no shared memory capabilities")
            return
        from sihd.core.Channel import ChannelQueue
        channel_test = ChannelQueue(name='test', mp=True)
        interactor.link("result", channel_test)
        self.assertTrue(interactor.start())
        time.sleep(0.1)
        interactor.new_interaction.write(url)
        time.sleep(1)
        req_http = channel_test.read()
        if req_http is None:
            time.sleep(1)
            req_http = channel_test.read()
        self.assertTrue(interactor.stop())
        self.assertTrue(req_http is not None)

if __name__ == '__main__':
    unittest.main()
