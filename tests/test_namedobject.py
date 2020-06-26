#!/usr/bin/python
# coding: utf-8

""" System """
import unittest
import sys

import sihd
logger = sihd.log.setup('info')

from sihd.Core.ANamedObject import ANamedObject
from sihd.Core.ANamedObjectContainer import ANamedObjectContainer

class TestNamedObject(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    def test_link_waterfall(self):
        root = ANamedObjectContainer(name='root')
        c1 = ANamedObjectContainer(name='container1', parent=root)
        c2 = ANamedObjectContainer(name='container2', parent=root)
        c3 = ANamedObjectContainer(name='container3', parent=root)
        c4 = ANamedObjectContainer(name='container4', parent=root)
        c4_item = ANamedObject('item', parent=c4)
        c1.link('item', '..container2.item')
        c2.link('item', '..container3.item')
        c3.link('item', '..container4.item')
        c1.process_links()
        root.print_tree()
        self.assertEqual(c1.get_child('item'), c4_item)
        self.assertEqual(c2.get_child('item'), c4_item)
        self.assertEqual(c3.get_child('item'), c4_item)
        self.assertEqual(c4.get_child('item'), c4_item)

    def test_links(self):
        struct = ANamedObjectContainer(name='struct')
        item1 = ANamedObject(name='item1', parent=struct)
        ANamedObject(name='item2', parent=struct)
        ANamedObject(name='item3', parent=struct)
        food = ANamedObjectContainer(name='food')
        pasta = ANamedObject(name='pasta', parent=food)
        ANamedObject(name='pizza', parent=food)
        ANamedObject(name='bologna', parent=food)
        struct.print_tree()
        food.print_tree()
        logger.info("Linking struct.item1 to food.pasta")
        struct.link("item1", "food.pasta")
        struct.print_tree()
        self.assertTrue(struct.is_linked('item1'))
        self.assertEqual(ANamedObjectContainer.root_find("struct.item1"), item1)
        self.assertNotEqual(struct.get_child("item1"), pasta)
        logger.info("Processing links")
        struct.process_links()
        struct.print_tree()
        self.assertEqual(struct.get_child("item1"), pasta)
        self.assertEqual(struct.get_child("item1").get_name(), "pasta")
        self.assertEqual(struct.get_child("item1").get_parent(), food)
        self.assertEqual(food.get_child("pasta"), pasta)
        self.assertEqual(ANamedObjectContainer.root_find("struct.item1"), pasta)

    def test_path_finding(self):
        parent1 = ANamedObjectContainer(name='parent1')
        child1 = ANamedObject(name='child1', parent=parent1)
        child2 = ANamedObject(name='child2', parent=parent1)

        parent2 = ANamedObjectContainer(name='parent2')
        evil_child = ANamedObject(name='child1', parent=parent2)
        self.assertEqual(ANamedObjectContainer.root_find("parent1.child1"), child1)
        self.assertEqual(ANamedObjectContainer.root_find("parent1.child2"), child2)
        self.assertEqual(ANamedObjectContainer.root_find("parent2.child1"), evil_child)
        self.assertEqual(ANamedObjectContainer.root_find("child1"), None)
        self.assertEqual(ANamedObjectContainer.root_find("child2"), None)

        self.assertEqual(ANamedObjectContainer.root_find("parent1"), parent1)
        self.assertEqual(ANamedObjectContainer.root_find("parent2"), parent2)

        subchild = ANamedObjectContainer(name='subchild', parent=parent1)
        subsubchild = ANamedObjectContainer(name='subsubchild', parent=subchild)
        self.assertEqual(subchild.find('..'), parent1)
        self.assertEqual(subchild.find('...'), None)
        self.assertEqual(subchild.find('..child1'), child1)

    def test_tree(self):
        """
            Expected hierarchy

            root: ANamedObjectContainer
              object2: ANamedObject
              object: ANamedObjectContainer
                list: ANamedObjectContainer
                  item1: ANamedObject
                  item2: ANamedObject
                  item3: ANamedObject
                  item4: ANamedObject
                  sub_lst: ANamedObjectContainer
                    item1: ANamedObject
                    item2: ANamedObject
                    item3: ANamedObject
                    item4: ANamedObject
        """
        root = ANamedObjectContainer(name='root')
        obj = ANamedObjectContainer(name='object', parent=root)
        lst = ANamedObjectContainer(name='list', parent=obj)
        ANamedObject(name='item1', parent=lst)
        ANamedObject(name='item2', parent=lst)
        ANamedObject(name='item3', parent=lst)
        ANamedObject(name='item4', parent=lst)
        sub_lst = ANamedObjectContainer(name='sub_lst', parent=lst)
        ANamedObject(name='item1', parent=sub_lst)
        ANamedObject(name='item2', parent=sub_lst)
        ANamedObject(name='item3', parent=sub_lst)
        ANamedObject(name='item4', parent=sub_lst)
        ANamedObject(name='object2', parent=root)
        root.print_tree()
        #testing tree
        dic = root.get_tree()
        self.assertEqual(dic['name'], 'root')
        #check hierarchy
        self.assertEqual(dic['children'][0]['name'], 'object2')
        self.assertEqual(dic['children'][1]['name'], 'object')
        self.assertEqual(dic['children'][1]\
                            ['children'][0]['name'], 'list')
        self.assertEqual(dic['children'][1]\
                            ['children'][0]\
                            ['children'][0]['name'], 'item1')
        self.assertEqual(dic['children'][1]\
                            ['children'][0]\
                            ['children'][4]['name'], 'sub_lst')
        self.assertEqual(dic['children'][1]\
                            ['children'][0]\
                            ['children'][4]\
                            ['children'][0]['name'], 'item1')
        #try finding stuff
        self.assertEqual(obj.find(''), None)
        self.assertEqual(obj.find('.'), None)
        self.assertEqual(obj.find('.list'), lst)
        self.assertEqual(lst.find('..'), obj)
        self.assertEqual(lst.find('...'), root)
        self.assertEqual(lst.find('...object'), obj)
        self.assertEqual(lst.find('...object.list'), lst)
        self.assertEqual(lst.find('...object.list.sub_lst'), sub_lst)
        self.assertNotEqual(lst.find('.item1'), None)
        self.assertNotEqual(lst.find('.item2'), None)
        self.assertNotEqual(lst.find('.item3'), None)
        self.assertNotEqual(lst.find('.item4'), None)
        self.assertEqual(lst.find('.item5'), None)
        self.assertNotEqual(lst.find('.sub_lst.item1'), None)
        #deleting main object
        ANamedObjectContainer.delete_from_tree('root.object')
        self.assertEqual(lst.get_parent(), obj)
        root.print_tree()
        dic = root.get_tree()
        self.assertEqual(dic['name'], 'root')
        self.assertEqual(len(dic['children']), 1)
        self.assertEqual(dic['children'][0]['name'], 'object2')


    def test_errors(self):
        parent = ANamedObjectContainer(name='parent')
        child1 = ANamedObject(name='child1', parent=parent)
        #Already in container root
        parent_error = None
        try:
            parent_error = ANamedObjectContainer(name='parent')
        except KeyError:
            pass
        self.assertTrue(parent_error is None)
        #Same key
        child_error = None
        try:
            child_error = ANamedObject(name='child1', parent=parent)
        except KeyError:
            pass
        self.assertTrue(child_error is None)
        #NamedObject cannot be parent
        child_error = None
        try:
            child_error = ANamedObject(name='err', parent=child1)
        except TypeError:
            pass
        self.assertTrue(child_error is None)

    def test_parent_child(self):
        parent = ANamedObjectContainer(name='parent')
        child1 = ANamedObject(name='child1', parent=parent)
        child2 = ANamedObject(name='child2', parent=parent)
        child3 = ANamedObject(name='child3', parent=parent)
        self.assertEqual(parent.get_child('child1'), child1)
        self.assertEqual(parent.get_child('child2'), child2)
        self.assertEqual(parent.get_child('child3'), child3)
        self.assertEqual(child1.get_parent(), parent)
        self.assertEqual(child2.get_parent(), parent)
        self.assertEqual(child3.get_parent(), parent)

        parent.print_tree()
        logger.info("Introducing evil parent")
        evil_parent = ANamedObjectContainer(name='evil_parent')
        self.assertEqual(evil_parent.get_child('child1'), None)
        child1.set_parent(evil_parent)
        self.assertNotEqual(child1.get_parent(), parent)
        self.assertEqual(child1.get_parent(), evil_parent)
        self.assertEqual(evil_parent.get_child('child1'), child1)
        self.assertEqual(parent.get_child('child1'), None)

        evil_parent.print_tree()
        parent.print_tree()
        logger.info("Deleting child2")
        refcount_child2 = sys.getrefcount(child2)
        child2.set_parent(None)
        self.assertEqual(refcount_child2, sys.getrefcount(child2) + 1)
        del child2
        self.assertEqual(parent.get_child('child2'), None)
        self.assertEqual(ANamedObjectContainer.root_find('parent.child2'), None)

        parent.print_tree()
        logger.info("Deleting parent")
        refcount_parent = sys.getrefcount(parent)
        child3.set_parent(None)
        self.assertEqual(refcount_parent, sys.getrefcount(parent) + 1)
        self.assertEqual(child3.get_parent(), None)
        del child3
        ANamedObjectContainer.delete_from_tree("parent")
        self.assertEqual(refcount_parent, sys.getrefcount(parent) + 2)
        del parent
        self.assertEqual(ANamedObjectContainer.root_find('parent'), None)
        self.assertEqual(ANamedObjectContainer.root_find('child3'), None)

        logger.info("Deleting evil parent child1")
        ANamedObjectContainer.delete_from_tree('evil_parent.child1')
        del child1
        self.assertEqual(ANamedObjectContainer.root_find('evil_parent.child1'), None)
        logger.info("Deleting evil parent")
        ANamedObjectContainer.delete_from_tree('evil_parent')
        self.assertEqual(ANamedObjectContainer.root_find('evil_parent'), None)
