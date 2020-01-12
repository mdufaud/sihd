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

class TestHttp(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_wrong_url(self):
        url = "http://www.a@E2e2esasfzzzzzzzzzzz.com"
        interactor = sihd.Interactors.ip.HttpInteractor()
        html = interactor.make_request(url).send()
        self.assertTrue(html is None)

    def test_get(self):
        url = "http://httpbin.org/get"
        interactor = sihd.Interactors.ip.HttpInteractor()
        html = interactor.make_request(url, query={"get": "cookies"}).send().decode()
        print(html)
        self.assertTrue(html is not None)

    def test_post(self):
        url = "http://httpbin.org/post"
        interactor = sihd.Interactors.ip.HttpInteractor()
        html = interactor.make_request(url, post={'hello': 'world'}).send().decode()
        print(html)
        self.assertTrue(html is not None)

    def test_error_404(self):
        url = "http://httpbin.org/status/300"
        interactor = sihd.Interactors.ip.HttpInteractor()
        html = interactor.make_request(url).send()
        print(html)
        self.assertTrue(html is None)


    def test_service(self):
        url = "https://www.google.com"
        interactor = sihd.Interactors.ip.HttpInteractor()
        interactor.set_conf({
            "url": url,
        })
        interactor.setup()
        interactor.start()
        time.sleep(0.2)
        self.assertTrue(interactor.stop())

if __name__ == '__main__':
    unittest.main()
