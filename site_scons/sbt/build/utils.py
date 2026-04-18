###############################################################################
# Env
###############################################################################

import os
import sys
from os import getenv

###############################################################################
# Filesystem
###############################################################################

def link_tree(src, dst):
    """Recursively create symlinks in dst for each file found under src."""
    if not os.path.isdir(src):
        return
    os.makedirs(dst, exist_ok=True)
    for entry in os.scandir(src):
        # Resolve to absolute real path to avoid chaining symlinks
        s = os.path.realpath(entry.path)
        d = os.path.join(dst, entry.name)
        if entry.is_dir(follow_symlinks=False):
            link_tree(entry.path, d)
        else:
            if os.path.islink(d) or os.path.exists(d):
                os.unlink(d)
            os.symlink(s, d)

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
