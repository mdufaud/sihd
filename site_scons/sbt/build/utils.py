###############################################################################
# Env
###############################################################################

import sys
from os import getenv

def get_opt(argname, default_val=""):
    arg_to_find = argname + "="
    for arg in sys.argv:
        idx = arg.find(arg_to_find)
        if idx >= 0:
            val = arg[idx + len(arg_to_find):]
            if len(val) > 0:
                return val
    return getenv(argname, "") or default_val

def is_opt(argname, default_val=""):
    ret = get_opt(argname, default_val)
    return ret == "1" or ret.lower() == "true"
