#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import sys
import time

import test_utils
import sihd

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

def test_action(url):
    interactor = sihd.Interactors.ip.HttpInteractor()
    return interactor.get(url)

def test_service(url):
    interactor = sihd.Interactors.ip.HttpInteractor()
    interactor.set_conf({
        "url": url,
        "type": "get"
    })
    interactor.setup()
    interactor.start()
    time.sleep(0.2)
    assert(interactor.stop())

if __name__ == '__main__':
    logger.info("Starting test")
    google = "https://www.google.com"
    test_service(google)
    assert(test_action(google) is not None)
    false_one = "http://www.a@E2e2esasfzzzzzzzzzzz.com"
    assert(test_action(false_one) is None)
    logger.info("Test ending")
