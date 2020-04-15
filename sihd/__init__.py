from .Core import *
from .Readers import *
from .Handlers import *
from .GUI import *
from .Interactors import *
from .App import *
from .API import *
from .Tools import *

__version__ = "0.0.1"

def set_log_color(activate):
    Core.ILoggable.set_color(activate)

def get(name, **kwargs):
    return Core.Factory.get(name, **kwargs)

def find(name, **kwargs):
    return Core.Factory.find(name, **kwargs)
