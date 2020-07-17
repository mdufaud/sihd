#!/usr/bin/python
# coding: utf-8

import sys
from os import getcwd, listdir
from os.path import join, exists, dirname, abspath, isfile, isdir, basename
glob = None

#
# Paths
#

resource_paths = ['.']

def add(path):
    path = abspath(path)
    if not path or path in resource_paths:
        return False
    resource_paths.append(path)
    return True

def remove(path):
    path = abspath(path)
    if not path or path in resource_paths:
        return False
    resource_paths.remove(path)
    return True

import sihd
sihd_path = dirname(sihd.__file__)
exe_path = dirname(sys.argv[0])
initial_wd = getcwd()
add(exe_path)
add(sihd_path)
add(initial_wd)

#
# Get
#

def get_dir(dirname, reverse=False):
    if not dirname:
        return
    apath = abspath(dirname)
    if exists(apath) and isfile(apath):
        return apath
    for path in resource_paths[::-1 if reverse else 1]:
        if basename(path) == dirname:
            return path

def get(filename, reverse=False, fileonly=False):
    if not filename:
        return
    apath = abspath(filename)
    if (exists(apath) and not fileonly) or isfile(apath):
        return apath
    for path in resource_paths[::-1 if reverse else 1]:
        output = abspath(join(path, filename))
        if (exists(output) and not fileonly) or isfile(output):
            return output

def get_file(filename):
    return get(filename, fileonly=True)

#
# Find
#

def _find_recursive(filename, directory):
    ret = None
    for dirname in listdir(directory):
        path = abspath(join(directory, dirname))
        output = abspath(join(path, filename))
        if exists(output) and isfile(output):
            ret = output
        elif isdir(path):
            ret = _find_recursive(filename, path)
        if ret is not None:
            break
    return ret

def find(filename, reverse=False):
    if not filename:
        return
    if exists(abspath(filename)):
        return abspath(filename)
    ret = None
    for dirname in resource_paths[::-1 if reverse else 1]:
        output = abspath(join(dirname, filename))
        if exists(output) and isfile(output):
            ret = output
        else:
            ret = _find_recursive(filename, dirname)
        if ret is not None:
            break
    return ret

def glob_find(expr, reverse=False, *args, **kwargs):
    global glob
    if glob is None:
        import glob
    ret = []
    for name in resource_paths[::-1 if reverse else 1]:
        lst = glob.glob(join(name, expr), *args, **kwargs)
        if lst:
            ret.extend(lst)
    return ret
