""" System """

import os
import sys
import subprocess

def find_first_interface():
    """ Find internet interface and returns it """
    wireless, dev = list_interfaces()
    for iface in wireless:
        if is_interface_monitoring(iface):
            return [iface]
    return None

def list_interfaces():
    lst = []
    wireless_lst = []
    if os.name == 'posix':
        for dir in os.listdir('/sys/class/net'):
            lst.append(dir)
        for iface in lst:
            if os.path.isdir('/sys/class/net/%s/wireless' % iface):
                wireless_lst.append(iface)
    return wireless_lst, lst

def is_interface_monitoring(iface):
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
