#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
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
        self.assertFalse(interactor.get(url))

    def test_action(self):
        url = "https://www.google.com"
        interactor = sihd.Interactors.ip.HttpInteractor()
        self.assertTrue(interactor.get(url))

    def test_service(self):
        url = "https://www.google.com"
        interactor = sihd.Interactors.ip.HttpInteractor()
        interactor.set_conf({
            "url": url,
            "type": "get"
        })
        interactor.setup()
        interactor.start()
        time.sleep(0.2)
        self.assertTrue(interactor.stop())

if __name__ == '__main__':
    unittest.main()
