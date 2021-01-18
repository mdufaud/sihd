#!/usr/bin/python
# coding: utf-8

import sys
from os import getcwd, listdir
from os.path import join, exists, dirname, abspath, isfile, isdir, basename
glob = None

#
# Paths
#

paths = []

def add(*path, verify=True):
    """ Adds a directory to resources path """
    if not path or None in path:
        return False
    path = abspath(join(*path))
    if verify is True and not isdir(path) or path in paths:
        return False
    paths.append(path)
    return True

def remove(*path):
    """ Remove a directory from resources path """
    if not path or None in path:
        return False
    path = abspath(join(*path))
    if path not in paths:
        return False
    paths.remove(path)
    return True

import sihd
sihd_path = dirname(sihd.__file__)
exe_path = dirname(sys.argv[0])
initial_wd = getcwd()
add(exe_path)
add(initial_wd)
add(sihd_path)

#
# Get
#

def get_path(dirname, reverse=False):
    """
        Get the path of a directory from resources path

        resources_path:
            /path/to/directory
            /second/path/to/another

        get_path("another") -> /second/path/to/another
    """
    if not dirname:
        return
    path = abspath(dirname)
    if exists(path) and isdir(path):
        return path
    for path in paths[::-1 if reverse else 1]:
        if isdir(path) and basename(path) == dirname:
            return path
    return None

def get(*filename, reverse=False, file=True, dir=True):
    """
        Get a file or a directory from resources path

        filesystem:
            /path/to/directory/file1
            /path/to/directory/file2
            /path/to/directory/dir/file

        resoures_path:
            /path/to/directory

        get("file1") -> /path/to/directory/file1
        get("dir", "file") -> /path/to/directory/dir/file
    """
    if not filename or None in filename:
        return
    output = abspath(join(*filename))
    if exists(output)\
        and ((file is True and isfile(output))\
            or (dir is True and isdir(output))):
        return output
    for path in paths[::-1 if reverse else 1]:
        output = abspath(join(path, *filename))
        if exists(output)\
            and ((file is True and isfile(output))\
                or (dir is True and isdir(output))):
            return output
    return None

def get_dir(*filename):
    return get(*filename, dir=True, file=False)

def get_file(*filename):
    return get(*filename, file=True, dir=False)

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
    for dirname in paths[::-1 if reverse else 1]:
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
    for name in paths[::-1 if reverse else 1]:
        lst = glob.glob(join(name, expr), *args, **kwargs)
        if lst:
            ret.extend(lst)
    return ret
