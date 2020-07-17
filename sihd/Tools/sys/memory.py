#!/usr/bin/python
# coding: utf-8

import os
try:
    import psutil
except ImportError:
    psutil = None

from sihd.Tools.sys.const import LINUX

def split_mem(bytes_mem):
    giga, remain = divmod(bytes_mem, 1E9)
    mega, remain = divmod(remain, 1E6)
    kilo, byte = divmod(remain, 1E3)
    return int(giga), int(mega), int(kilo), int(byte)

def usage_bytes():
    if psutil:
        process = psutil.Process(os.getpid())
        return process.memory_info()[0]
    if LINUX:
        ''' Memory usage in kB '''
        with open('/proc/self/status') as f:
            memusage = f.read().split('VmRSS:')[1].split('\n')[0][:-3]
        return int(memusage.strip()) * 1E3
    return 0

def usage():
    mem = usage_bytes()
    return split_mem(mem)

def usage_format(mem=None):
    s = "Mem: "
    mem = mem or usage_bytes()
    giga, mega, kilo, byte = split_mem(mem)
    if giga:
        s += "{:d} {:>3d} Mb".format(giga, mega)
    elif mega:
        s += "{:>3d} Mb".format(mega)
    elif kilo:
        s += "{:>3d} Kb".format(kilo)
    else:
        s += "{:>3d} b".format(byte)
    return s
