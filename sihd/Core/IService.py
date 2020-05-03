#!/usr/bin/python
#coding: utf-8

class IService(object):

    def __init__(self):
        super().__init__()

    def start(self):
        raise NotImplementedError("start")

    def stop(self):
        raise NotImplementedError("stop")

    def pause(self):
        raise NotImplementedError("pause")

    def resume(self):
        raise NotImplementedError("resume")
