try:
    from .Curses import *
except ImportError:
    pass
try:
    from .Kivy import *
except ImportError:
    pass
try:
    from .WxPython import *
except ImportError:
    pass
