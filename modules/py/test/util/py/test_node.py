import sihd

root = sihd.util.Node("root")
parent1 = sihd.util.Node("parent1", root)
parent2 = sihd.util.Node("parent2")
sub_parent1 = sihd.util.Node("sub_parent", parent1)
sub_parent2 = sihd.util.Node("sub_parent", parent2)
named1 = sihd.util.Named("child", sub_parent1)
named2 = sihd.util.Named("child", sub_parent2)

named_parentless = sihd.util.Named("orphan")

assert(root.find("toto") == None)
assert(root.get_child("toto") == None)
assert(root.get_child("parent1") == parent1)
root.add_child_name("toto", parent2)
assert(root.find("toto") == parent2)

print(root.tree_str())

assert(root.name() == "root")
assert(named1.name() == "child")

assert(root != parent1)
assert(root.find("parent1") == parent1)

assert(parent1.find("..") == root)
assert(parent1.root() == root)
assert(parent1.parent() == root)

print("root children")
children = root.children()
for name, child_entry in children.items():
    print("  child: " + name + " - true name: " + child_entry.obj.name())

assert(root.children_keys() == ['parent1', 'toto'])
