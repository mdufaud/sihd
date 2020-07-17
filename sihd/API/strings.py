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

def add(key, s=None):
    """ Can add either a dict, list or key/value pair only in current language """
    if isinstance(key, dict):
        for dkey, dvalue in dic.items():
            current[dkey] = dvalue
    elif isinstance(key, (list, tuple, set)):
        for tupl in key:
            current[tupl[0]] = tupl[1]
    else:
        current[key] = s

def get(key):
    """ Get key only for current language """
    return current[key]

#
# Load/dump
#

def load(dic, lang=None):
    lang = lang or current_language
    __add_lang(lang)
    languages[lang].update(dic)

def load_json(json_data, lang=None):
    lang = lang or current_language
    dic = json.loads(json_data)
    load(dic, lang)

def dump_lang(filename, lang=None):
    lang = lang or current_language
    dic = languages[lang]
    json_data = json.dumps(dic)
    with open(filename, 'w') as file:
        json.dump(json_data, file)

def load_lang(filename, lang=None):
    lang = lang or current_language
    with open(filename, 'r') as file:
        json_data = json.load(file)
        load_json(json_data, lang)
