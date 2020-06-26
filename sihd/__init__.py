__version__ = "0.0.2"

_traceback = None

from .Tools import network
from .Tools import shell
from .Tools import sys
from .Tools import pcap
from .Tools import term
from .API import log
from .API import containers as tree

def get_traceback():
    global _traceback
    if _traceback is None:
        import traceback as _traceback
    return _traceback.format_exc()

""" Factory """

def get_cls(name, **kwargs):
    return Factory.get(name, **kwargs)

def find_cls(name, **kwargs):
    return Factory.find(name, **kwargs)

""" Resources """

def get_mem():
    return Tools.sys.memory.get_current_mem()
