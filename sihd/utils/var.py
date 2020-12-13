#!/usr/bin/python
# coding: utf-8

import re

def path_assign(val, key, value):
    if isinstance(val, dict):
        val[key] = value
    elif isinstance(val, (list, tuple)):
        val[int(key)] = value
    else:
        raise KeyError("Cannot assign key '{}' for type {}".format(key, type(val)))

def path_browse(val, key):
    if isinstance(val, dict):
        return val[key]
    elif isinstance(val, (list, tuple)):
        return val[int(key)]
    else:
        raise KeyError("Cannot browse further with key {} for type {}'"\
                       .format(key, type(val)))

def deep_pset(val, keylst, value):
    try:
        deep_set(val, keylst, value)
    except (KeyError, IndexError):
        return False
    return True

def deep_set(val, keylst, value):
    '''
        :param val: Can be either a list or dict
        :param keylst: Can be either a list or string

        Goes through dict/lst to set a value
        dict = {'key': {'subkey': [42, 24], 'val': 1 } }
        set(dict, 'key.subkey.1', X)
        set(dict, 'key.val', Y)
        set(dict, ['key', 'val'], Y)
    '''
    if isinstance(keylst, str):
        keylst = keylst.split('.')
    if not keylst:
        return
    for key in keylst[:-1]:
        val = path_browse(val, key)
    path_assign(val, keylst[-1], value)

def deep_pfind(val, keylst, retval=None):
    try:
        return deep_find(val, keylst)
    except (KeyError, IndexError):
        return retval

def deep_find(val, keylst):
    '''
        :param val: Can be either a list or dict
        :param keylst: Can be either a list or string

        Goes through dict/lst
        dict = {'key': {'subkey': [42, 24], 'val': 1 } }
        find(dict, 'key.subkey.1') -> 24
        find(dict, 'key.val') -> 1
        find(dict, ['key', 'val']) -> 1
    '''
    if isinstance(keylst, str):
        keylst = keylst.split('.')
    if not keylst:
        return
    for key in keylst:
        val = path_browse(val, key)
    return val

def from_string(s):
    if s.find('.') >= 0:
        try:
            return float(s)
        except ValueError:
            pass
    try:
        return int(s)
    except ValueError:
        pass
    if s[0] == "'" or s[0] == '"':
        return s[1:-1]
    low = s.lower()
    if low == 'true':
        return True
    elif low == 'false':
        return False
    return s

def tokenize(string, token=';'):
    '''
        :param string: token separated string
        :param token: token that separates elements

        Transform: 'key=value;key2=value2;...'
        Into: {'key': 'value', 'key2': 'value2', ...}
    '''
    if not isinstance(string, str) or not string:
        return {}
    ret = {}
    # https://stackoverflow.com/questions/2785755/how-to-split-but-ignore-separators-in-quoted-strings-in-python
    elements = re.split('''{}(?=(?:[^'"]|'[^']*'|"[^"]*")*$)'''.format(token), string)
    for i, el in enumerate(elements):
        split = el.split('=')
        if len(split) == 2:
            ret[split[0]] = from_string(split[1])
        else:
            raise ValueError("Tokenize error on token {}: '{}' -> '{}'".format(i, el, string))
    return ret
