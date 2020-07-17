#!/usr/bin/python
# coding: utf-8

import os
import sys
import subprocess

def find_first_monitoring():
    """ Find internet interface and returns it """
    wireless = list_wifi()
    for iface in wireless:
        if is_interface_monitoring(iface):
            return iface

def list():
    lst = []
    if os.name == 'posix':
        for dirname in os.listdir('/sys/class/net'):
            lst.append(dirname)
    return lst

def list_wifi():
    wireless_lst = []
    if os.name == 'posix':
        for dirname in os.listdir('/sys/class/net'):
            if os.path.isdir('/sys/class/net/%s/wireless' % dirname):
                wireless_lst.append(dirname)
    return wireless_lst

def is_monitoring(iface):
    """
        http://lxr.free-electrons.com/source/include/uapi/linux/if_arp.h

        Managed Mode: Type = 1 (ARPHRD_ETHER)
        Monitor Mode: Type = 803 (ARPHRD_IEEE80211_RADIOTAP)
    """
    if os.name == 'posix':
        path = "/sys/class/net/%s/type" % iface
        if not os.path.isdir(path):
            return False
        with open(path, 'r') as f:
            t = int(f.read())
            if t == 1:
                return False
            elif t == 803:
                return True
        return False
    return False
