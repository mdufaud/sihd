from .Core import *
from .Readers import *
from .Handlers import *
from .GUI import *
from .Interactors import *
from .App import *
from .API import *
from .Tools import *

from .Core.ANamedObjectContainer import ANamedObjectContainer
from .Core.ALoggable import ALoggable
from .Core.Factory import Factory

__version__ = "0.0.2"

_traceback = None

logger = ALoggable.logger

""" Log fast setup """

def set_log(level='info', stream=None):
    ALoggable.add_stream_handler(level=level, stream=stream)
    ALoggable.set_color(True)
    return ALoggable.logger

def set_log_color(activate):
    ALoggable.set_color(activate)

def get_log():
    return ALoggable.logger

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

""" Named objects """

def add_container(name, parent=None):
    return ANamedObjectContainer(name, parent)

def find(path):
    return ANamedObjectContainer.root_find(path)

def clear_tree():
    ANamedObjectContainer.clear_tree()

def delete_from_tree(self, path):
    ANamedObjectContainer.delete_from_tree(path)
