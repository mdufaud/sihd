#!/usr/bin/python
#coding: utf-8

""" System """
import time

import logging
logger = logging.getLogger("sihd")

try:
    import multiprocessing
    from multiprocessing import Process, Manager
    ret = multiprocessing.Value('i', 0)
except (ImportError, FileNotFoundError):
    multiprocessing = None

def is_multiprocessing():
    return multiprocessing is not None

def __proc_channel_write(channel, value, value2):
    global ret
    if value2 is not None:
        ret.value = channel.write(value, value2)
    else:
        ret.value = channel.write(value)

def write_channel_mp(channel, value, value2=None):
    p = Process(target=__proc_channel_write, args=[channel, value, value2])
    p.start()
    p.join()
    return bool(ret.value)

def write_channel(channel, value, value2=None):
    logger.info("Writing -> {}{}".format(value,
        " - " + str(value2) if value2 is not None else ""))
    if channel.is_multiprocess():
        ret = write_channel_mp(channel, value, value2)
    else:
        if value2 is not None:
            ret = channel.write(value, value2)
        else:
            ret = channel.write(value)
    return ret
