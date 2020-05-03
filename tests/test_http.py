#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import unittest
import json

import utils
import sihd
logger = sihd.set_log('debug')

from sihd.Interactors.ip.HttpInteractor import HttpInteractor

from sihd.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self.add_channel_input("input")

    def handle(self, channel):
        data = channel.read()
        if data:
            self.out = data.decode()
            print("Received: ", self.out)
        return True

class TestHttp(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_wrong_url(self):
        logger.info("Testing wrong url")
        url = "http://www.a@E2e2esasfzzzzzzzzzzz.com"
        interactor = HttpInteractor()
        html = interactor.make_request(url).send()
        self.assertTrue(html is None)

    def test_error_404(self):
        logger.info("Testing error 404")
        url = "http://httpbin.org/status/404"
        interactor = HttpInteractor()
        html = interactor.make_request(url).send()
        self.assertTrue(html is None)

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

    def test_post(self):
        logger.info("Testing POST")
        url = "http://httpbin.org/post"
        interactor = HttpInteractor()
        json_data = interactor.make_request(url, post={'hello': 'world'}).send().decode()
        self.assertTrue(json_data is not None)
        dic = json.loads(json_data)
        self.assertEqual(dic["form"]['hello'], 'world')

    def test_service(self):
        logger.info("Testing service life cycle")
        url = "https://www.google.com"
        interactor = HttpInteractor()
        interactor.set_conf({
            "url": url,
            'runnable_type': 'thread'
        })
        self.assertTrue(interactor.setup())
        self.assertTrue(interactor.start())
        time.sleep(1)
        self.assertTrue(interactor.result.read() is not None)
        self.assertTrue(interactor.stop())

    @unittest.skipIf(utils.is_multiprocessing() is False, "No support for multiprocess")
    def test_channels(self):
        logger.info("Testing channels well being")
        url = "https://www.google.com"
        interactor = HttpInteractor()
        interactor.set_conf('runnable_type', 'process')
        try:
            self.assertTrue(interactor.setup())
        except FileNotFoundError:
            #/dev/shm
            logger.warning("Test cannot continue as your device has no shared memory capabilities")
            return
        channel_test = sihd.ChannelQueue(name='test', mp=True)
        interactor.link_channel("result", channel_test)
        self.assertTrue(interactor.start())
        time.sleep(0.1)
        interactor.new_interaction.write(url)
        time.sleep(2)
        self.assertTrue(interactor.stop())
        self.assertTrue(channel_test.read() is not None)

if __name__ == '__main__':
    unittest.main()
