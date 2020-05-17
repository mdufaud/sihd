#!/usr/bin/python
#coding: utf-8

""" System """
import unittest
import sys

import sihd
logger = sihd.set_log('debug')

from sihd.Core.ANamedObject import ANamedObject

class TestNamedObject(unittest.TestCase):

    def setUp(self):
        print()
        sihd.clear_tree()

    def tearDown(self):
        pass

    def test_links(self):
        struct = ANamedObject(name='struct')
        item1 = ANamedObject(name='item1', parent=struct)
        ANamedObject(name='item2', parent=struct)
        ANamedObject(name='item3', parent=struct)
        food = ANamedObject(name='food')
        pasta = ANamedObject(name='pasta', parent=food)
        ANamedObject(name='pizza', parent=food)
        ANamedObject(name='bologna', parent=food)
        print(struct.dump_tree())
        print(food.dump_tree())
        logger.info("Linking struct.item1 to food.pasta")
        struct.link("item1", "food.pasta")
        print(struct.dump_tree())
        self.assertTrue(struct.is_linked('item1'))
        self.assertEqual(ANamedObject.find("struct.item1"), item1)
        self.assertNotEqual(struct.get_child("item1"), pasta)
        logger.info("Processing links")
        struct.process_links()
        print(struct.dump_tree())
        self.assertEqual(struct.get_child("item1"), pasta)
        self.assertEqual(struct.get_child("item1").get_name(), "pasta")
        self.assertEqual(struct.get_child("item1").get_parent(), food)
        self.assertEqual(food.get_child("pasta"), pasta)
        self.assertEqual(ANamedObject.find("struct.item1"), pasta)

    def test_path_finding(self):
        parent1 = ANamedObject(name='parent1')
        child1 = ANamedObject(name='child1', parent=parent1)
        child2 = ANamedObject(name='child2', parent=parent1)

        parent2 = ANamedObject(name='parent2')
        evil_child = ANamedObject(name='child1', parent=parent2)
        self.assertEqual(ANamedObject.find("parent1.child1"), child1)
        self.assertEqual(ANamedObject.find("parent1.child2"), child2)
        self.assertEqual(ANamedObject.find("parent2.child1"), evil_child)
        self.assertEqual(ANamedObject.find("child1"), None)
        self.assertEqual(ANamedObject.find("child2"), None)

        self.assertEqual(ANamedObject.find("parent1"), parent1)
        self.assertEqual(ANamedObject.find("parent2"), parent2)

        child_error = None
        try:
            child_error = ANamedObject(name='child1', parent=parent1)
        except ValueError:
            pass
        self.assertTrue(child_error is None)

    def test_tree(self):
        root = ANamedObject(name='root')
        obj = ANamedObject(name='object', parent=root)
        lst = ANamedObject(name='list', parent=obj)
        ANamedObject(name='item1', parent=lst)
        ANamedObject(name='item2', parent=lst)
        ANamedObject(name='item3', parent=lst)
        ANamedObject(name='item4', parent=lst)
        sub_lst = ANamedObject(name='sub_lst', parent=lst)
        ANamedObject(name='item1', parent=sub_lst)
        ANamedObject(name='item2', parent=sub_lst)
        ANamedObject(name='item3', parent=sub_lst)
        ANamedObject(name='item4', parent=sub_lst)
        ANamedObject(name='object2', parent=root)
        print(root.dump_tree())
        ANamedObject.delete_from_tree('root.object')
        self.assertEqual(lst.get_parent(), obj)
        print(root.dump_tree())

    def test_parent_child(self):
        parent = ANamedObject(name='parent')
        child1 = ANamedObject(name='child1', parent=parent)
        child2 = ANamedObject(name='child2', parent=parent)
        child3 = ANamedObject(name='child3', parent=parent)
        self.assertEqual(parent.get_child('child1'), child1)
        self.assertEqual(parent.get_child('child2'), child2)
        self.assertEqual(parent.get_child('child3'), child3)
        self.assertEqual(child1.get_parent(), parent)
        self.assertEqual(child2.get_parent(), parent)
        self.assertEqual(child3.get_parent(), parent)

        print(parent.dump_tree())
        logger.info("Introducing evil parent")
        evil_parent = ANamedObject(name='evil_parent')
        self.assertEqual(evil_parent.get_child('child1'), None)
        child1.set_parent(evil_parent)
        self.assertNotEqual(child1.get_parent(), parent)
        self.assertEqual(child1.get_parent(), evil_parent)
        self.assertEqual(evil_parent.get_child('child1'), child1)
        self.assertEqual(parent.get_child('child1'), None)

        print(evil_parent.dump_tree())
        print(parent.dump_tree())
        logger.info("Deleting child2")
        refcount_child2 = sys.getrefcount(child2)
        child2.set_parent(None)
        self.assertEqual(refcount_child2, sys.getrefcount(child2) + 1)
        del child2
        self.assertEqual(parent.get_child('child2'), None)
        self.assertEqual(ANamedObject.find('parent.child2'), None)

        print(parent.dump_tree())
        logger.info("Deleting parent")
        refcount_parent = sys.getrefcount(parent)
        child3.set_parent(None)
        self.assertEqual(refcount_parent, sys.getrefcount(parent) + 1)
        ANamedObject.delete_from_tree("parent")
        self.assertEqual(child3.get_parent(), None)
        del child3
        self.assertEqual(refcount_parent, sys.getrefcount(parent) + 2)
        del parent
        self.assertEqual(ANamedObject.find('parent'), None)
        self.assertEqual(ANamedObject.find('child3'), None)

        logger.info("Deleting evil parent child1")
        ANamedObject.delete_from_tree('evil_parent.child1')
        del child1
        self.assertEqual(ANamedObject.find('evil_parent.child1'), None)
        logger.info("Deleting evil parent")
        ANamedObject.delete_from_tree('evil_parent')
        self.assertEqual(ANamedObject.find('evil_parent'), None)
