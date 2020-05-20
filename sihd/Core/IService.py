#!/usr/bin/python
#coding: utf-8

class IService(object):

    def init(self):
        raise NotImplementedError("init")

    def start(self):
        raise NotImplementedError("start")

    def stop(self):
        raise NotImplementedError("stop")

    def pause(self):
        raise NotImplementedError("pause")

    def resume(self):
        raise NotImplementedError("resume")

    def reset(self):
        raise NotImplementedError("reset")
