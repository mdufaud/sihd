#!/usr/bin/python
# coding: utf-8

__version__ = "0.0.2"

# Before because of term color usage in other modules
from .utils import term
from .utils import tree, network, sys, pcap, shell, resources, strings, stats, var, logger

log = logger.LoggerApi("")

def get_path():
    dirname = resources.dirname
    return dirname(dirname(__file__))

from os import getenv
debug = getenv("SIHD_DEBUG") == "1"
if debug:
    log.set_level("debug")
