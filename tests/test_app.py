#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import sys

import test_utils
import sihd

x = 1

def assert_expected(app, lines, skipped,
                    check_words, prt=False):
    reader = app._line_reader
    handler = app._word_handler 
    words_dict = handler._stats

    for key, value in words_dict.items():
        if prt:
            print("{}: {}".format(key, value))
        expected = check_words.get(key, None)
        if expected is not None:
            assert(expected == value), "{}: {} != {}".format(key, value, expected)

    assert(handler._skipped == skipped), "{} != {}".format(handler._skipped, skipped)
    assert(reader._lines == lines), "{} != {}".format(reader._lines, lines)
    assert(reader._fully_read)

    if not app.args.stats:
        return
    print()
    for key, obj in sihd.Utilities.Stats.get_stats().items():
        print(obj)
    print()
    sihd.Utilities.Stats.reset()

def test_file(path, lines, skipped, check_words={}):
    global x
    print("Test-{} with file '{}' with {} lines and {} comments".format(x, path, lines, skipped))
    app = test_utils.TestApp(x)
    x += 1
    if not app.is_args():
        app.set_args([
            "-f", path,
            "-s",
        ])
    if app.setup() is False:
        sys.exit(1)
    app.start()
    app.loop(timeout=1)
    assert_expected(app, lines, skipped, check_words, prt=False)

if __name__ == '__main__':
    try:
        dir_path = os.path.join(os.path.dirname(__file__), "resources", "Txt")
        test_file(os.path.join(dir_path, "5_lines.txt"), 5, 0, {"world": 2})
        test_file(os.path.join(dir_path, "comments_and_empty_lines.txt"), 19, 6, {"A": 2})
    except KeyboardInterrupt as e:
        sys.exit(1)
