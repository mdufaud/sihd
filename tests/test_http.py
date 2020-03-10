#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time

import sihd
import unittest

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

import json

from sihd.Interactors.ip.HttpInteractor import HttpInteractor
from sihd.Readers.ip.HttpReader import HttpReader

from sihd.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)

    def handle(self, reader, data):
        self.out = data.decode()
        print("Received: ", self.out)
        return True

class TestHttp(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_wrong_url(self):
        url = "http://www.a@E2e2esasfzzzzzzzzzzz.com"
        interactor = HttpInteractor()
        html = interactor.make_request(url).send()
        self.assertTrue(html is None)

    def test_error_404(self):
        url = "http://httpbin.org/status/404"
        interactor = HttpInteractor()
        html = interactor.make_request(url).send()
        self.assertTrue(html is None)

    def test_get(self):
        url = "http://httpbin.org/get"
        interactor = HttpInteractor()
        json_data = interactor.make_request(url, query={"get": "cookies"}).send().decode()
        print(json_data)
        self.assertTrue(json_data is not None)
        dic = json.loads(json_data)
        self.assertEqual(dic["url"], "{}?get=cookies".format(url))
        self.assertEqual(dic["args"]['get'], 'cookies')

    def test_post(self):
        url = "http://httpbin.org/post"
        interactor = HttpInteractor()
        json_data = interactor.make_request(url, post={'hello': 'world'}).send().decode()
        self.assertTrue(json_data is not None)
        dic = json.loads(json_data)
        self.assertEqual(dic["form"]['hello'], 'world')

    def test_service(self):
        url = "https://www.google.com"
        interactor = HttpInteractor()
        interactor.set_conf({
            "url": url,
        })
        self.assertTrue(interactor.setup())
        self.assertTrue(interactor.start())
        time.sleep(0.2)
        self.assertTrue(interactor.stop())

    def test_reader(self):
        url = "http://httpbin.org/get"
        reader = HttpReader()
        handler = TestHandler()
        reader.set_conf({
            "url": url,
        })
        reader.add_observer(handler)
        self.assertTrue(reader.setup())
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(0.3)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        dic = json.loads(handler.out)
        self.assertEqual(dic["url"], url)

if __name__ == '__main__':
    unittest.main()
