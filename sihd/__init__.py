from .Core import *
from .Readers import *
from .Handlers import *
from .GUI import *
from .Interactors import *
from .App import *
from .API import *
from .Tools import *

from .Core.ANamedObject import ANamedObject

__version__ = "0.0.2"

_traceback = None

""" Log fast setup """

def set_log(level='info', stream=None):
    Core.ALoggable.add_stream_handler(level=level, stream=stream)
    Core.ALoggable.set_color(True)
    return Core.ALoggable.logger

def set_log_color(activate):
    Core.ALoggable.set_color(activate)

def get_log():
    return Core.ALoggable.logger

def get_traceback():
    global _traceback
    if _traceback is None:
        import traceback as _traceback
    return _traceback.format_exc()

""" Factory fast usage """

def get_cls(name, **kwargs):
    return Core.Factory.get(name, **kwargs)

def find_cls(name, **kwargs):
    return Core.Factory.find(name, **kwargs)

""" Resources """

def get_mem():
    return Tools.sys.memory.get_current_mem()

""" Named objects """

def clear_tree():
    ANamedObject.clear_tree()
