#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys

import test_utils
import sihd
import unittest

class TestApp(unittest.TestCase):

    def setUp(self):
        self.x = 0
        pass

    def tearDown(self):
        pass

    def file_expect(self, app, lines, skipped,
                    check_words, prt=False):
        reader = app._line_reader
        handler = app._word_handler 
        words_dict = handler._stats

        for key, value in words_dict.items():
            if prt:
                print("{}: {}".format(key, value))
            expected = check_words.get(key, None)
            if expected is not None:
                self.assertEqual(expected, value), "{}: {} != {}".format(key, value, expected)

        self.assertEqual(handler._skipped, skipped), "{} != {}".format(handler._skipped, skipped)
        self.assertEqual(reader._lines, lines), "{} != {}".format(reader._lines, lines)
        self.assertTrue(reader._fully_read)

        if not app.args.stats:
            return
        print()
        for key, obj in sihd.Core.Stats.get_stats().items():
            print(obj)
        print()
        sihd.Core.Stats.reset()

    def do_file(self, path, lines, skipped, check_words={}):
        print("Test-{} with file '{}' with {} lines and {} comments".format(self.x, path, lines, skipped))
        app = test_utils.TestApp(self.x)
        #app.set_path(path)
        self.x += 1
        if not app.is_args():
            app.set_args([
                "-f", path,
                "-s",
            ])
        if app.setup_app() is False:
            sys.exit(1)
        app.start_all()
        app.loop(timeout=1)
        app.stop()
        self.file_expect(app, lines, skipped, check_words, prt=False)

    def test_file_reader(self):
        dir_path = os.path.join(os.path.dirname(__file__), "resources", "Txt")
        self.do_file(os.path.join(dir_path, "5_lines.txt"), 5, 0, {"world": 2})
        self.do_file(os.path.join(dir_path, "comments_and_empty_lines.txt"), 19, 6, {"A": 2})

if __name__ == '__main__':
    unittest.main(verbosity=2)
