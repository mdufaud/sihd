local root = sihd.util.Node("root", nil)
local child = sihd.util.Named("child", root)
assert(child:get_name() == "child")
assert(root:get_name() == "root")
assert(root:get_child("child") == child)
assert(child:get_parent() == root)
assert(child:get_root() == root)
assert(child:find("..") == root)

local second_child = sihd.util.Named("second_child", nil)
root:add_child_name("toto", second_child)

assert(second_child:get_name() == "second_child")
assert(second_child:get_parent() == root)
assert(second_child:get_root() == root)

print(root:get_tree_desc_str())

local children_tbl = root:get_children()
assert(children_tbl["child"] ~= nil)
assert(children_tbl["child"].name == "child")
assert(children_tbl["toto"] ~= nil)
assert(children_tbl["toto"].obj:get_name() == "second_child")

local children_keys = root:get_children_keys()
assert(children_keys[1] == "child")
assert(children_keys[2] == "toto")