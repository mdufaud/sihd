#!/usr/bin/python
#coding: utf-8

""" System """

class IServiceStateObserver(object):

    def service_state_changed(self, service, stopped, paused):
        raise NotImplementedError("Service state observer not implemented")
