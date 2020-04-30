#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import unittest

import utils
import sihd

from sihd.Tools.sys import memory

class TestApp(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    def file_expect(self, app, lines, skipped,
                    check_words, prt=False):
        reader = app._line_reader
        handler = app._word_handler
        words_dict = handler.output.get_data()

        self.assertTrue(words_dict)
        for key, value in words_dict.items():
            if prt:
                print("{}: {}".format(key, value))
            expected = check_words.get(key, None)
            if expected is not None:
                self.assertEqual(expected, value)

        self.assertEqual(handler.skipped.read(), skipped), "{} != {}".format(handler.skipped.read(), skipped)
        self.assertEqual(reader.lines.read(), lines), "{} != {}".format(reader.lines.read(), lines)
        self.assertTrue(reader.eof.read())

        if not app.args.stats:
            return
        print()
        for key, obj in sihd.Core.Stats.get_stats().items():
            print(obj)
        print()
        sihd.Core.Stats.reset()

    def do_file(self, path, lines, skipped, check_words={}):
        print("Test with file '{}' with {} lines and {} comments"\
                .format(path, lines, skipped))
        app = utils.TestApp(0)
        app.set_args([
            "-f", path,
            "-s",
        ])
        if app.setup_app() is False:
            sys.exit(1)
        app.start_all()
        app.loop(timeout=1)
        app.stop()
        #os.remove(app.get_conf_path())
        self.file_expect(app, lines, skipped, check_words, prt=False)

    def test_file_reader(self):
        dir_path = os.path.join(os.path.dirname(__file__), "resources", "Txt")
        self.do_file(os.path.join(dir_path, "5_lines.txt"), 5, 0, {"world": 2})

    def test_file_reader_2(self):
        dir_path = os.path.join(os.path.dirname(__file__), "resources", "Txt")
        self.do_file(os.path.join(dir_path, "comments_and_empty_lines.txt"), 10, 6, {"A": 2})


if __name__ == '__main__':
    unittest.main(verbosity=2)
