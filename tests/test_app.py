#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import unittest

import utils
import sihd

from sihd.utils.sys import memory


class TestAppApi(unittest.TestCase):

    def setUp(self):
        sihd.resources.add("tests", "resources", "txt")
        print()
        sihd.tree.clear()

    def tearDown(self):
        sihd.resources.remove("tests", "resources", "txt")
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

        self.assertEqual(handler.skipped.read(), skipped)
        self.assertEqual(reader.count.read(), lines)
        self.assertTrue(reader.eof.read())

        if not app.args.stats:
            return
        print()
        for key, obj in sihd.stats.get().items():
            print(obj)
        print()
        sihd.stats.reset()

    def do_file(self, path, lines, skipped, check_words={}):
        print("Test with file '{}' with {} lines and {} comments"\
                .format(path, lines, skipped))
        byte_before = sihd.sys.memory.usage_bytes()
        app = utils.TestApp()
        app.set_args([
            "-f", path,
            "-s",
        ])
        self.assertTrue(app.setup_app())
        self.assertTrue(app.start())
        app.loop(timeout=10)
        self.assertTrue(app.stop())
        #os.remove(app.get_conf_path())
        self.file_expect(app, lines, skipped, check_words, prt=False)
        byte_after = sihd.sys.memory.usage_bytes()
        self.assertTrue(app.reset())
        app.log_info(sihd.sys.memory.usage_format(byte_after - byte_before))

    def test_file_reader(self):
        self.do_file(sihd.resources.get("5_lines.txt"), 5, 0, {"world": 2})

    def test_file_reader_2(self):
        self.do_file(sihd.resources.get("comments_and_empty_lines.txt"), 10, 6, {"A": 2})

    def test_life_cycle(self):
        app = utils.TestApp()
        app.set_args([
            "-f", sihd.resources.get_file("5_lines.txt"),
            "-s",
        ])
        self.assertTrue(app.setup_app())
        self.assertTrue(app.start())
        app.loop(timeout=0.1)
        self.assertTrue(app.stop())
        app.print_tree()
        self.assertTrue(app.reset())
        app.print_tree()
        self.assertTrue(app.setup_app())
        self.assertTrue(app.start())
        app.loop(timeout=0.1)
        self.assertTrue(app.stop())
        app.print_tree()
        self.assertTrue(app.reset())
        app.print_tree()

if __name__ == '__main__':
    unittest.main(verbosity=2)
