#!/usr/bin/python
# coding: utf-8

import locale
import json

default_language, encoding = locale.getdefaultlocale()
languages = {
    default_language: {}
}
current_language = default_language
current = languages[current_language]

#
# Langs
#

def __add_lang(lang):
    if lang not in languages:
        languages[lang] = {}

def get_langs():
    return languages.keys()

def get_lang():
    return current_language

def set_lang(lang):
    global current_language
    current_language = lang
    __add_lang(lang)
    global current
    current = languages[lang]

def is_lang(lang):
    return lang in languages

#
# String keys
#

def is_key(key, lang=None):
    lang = lang or current_language
    return key in languages[lang]

def is_keys(keys, lang=None):
    lang = lang or current_language
    l = languages[lang]
    return all(key in l for key in keys)

def set(key, s):
    """ Can add either a dict, list or key/value pair only in current language """
    if not isinstance(s, str):
        raise ValueError("Not a string for lang={} key={} -> {}"\
                         .format(current_language, key, s))
    if isinstance(key, dict):
        for dkey, dvalue in dic.items():
            current[dkey] = dvalue
    elif isinstance(key, (list, tuple)):
        for el in key:
            current[el[0]] = el[1]
    else:
        current[key] = s

def default(key, s):
    """ Adds key if not already here """
    if key in current:
        return
    set(key, s)

def get(key, ret=None):
    """ Get key only for current language """
    return current.get(key, ret)

def get_format(key, *args, **kwargs):
    s = current.get(key, None)
    if isinstance(s, str):
        return s.format(*args, **kwargs)
    return s

#
# Load/dump
#

def load(dic, lang=None):
    lang = lang or current_language
    __add_lang(lang)
    ok = all(isinstance(v, str) for v in dic.values())
    if not ok:
        raise ValueError("Could not load as some elements were not strings")
    languages[lang].update(dic)

def load_json(json_data, lang=None):
    lang = lang or current_language
    dic = json.loads(json_data)
    load(dic, lang)

def load_lang(filename, lang=None):
    lang = lang or current_language
    with open(filename, 'r') as file:
        json_data = json.load(file)
        load_json(json_data, lang)

def dump_lang(filename, lang=None):
    lang = lang or current_language
    dic = languages[lang]
    json_data = json.dumps(dic)
    with open(filename, 'w') as file:
        json.dump(json_data, file)
