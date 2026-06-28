local root = sihd.util.Node("root", nil)
local child = sihd.util.Named("child", root)
assert(child:name() == "child")
assert(root:name() == "root")
assert(root:get_child("child") == child)
assert(child:parent() == root)
assert(child:root() == root)
assert(child:find("..") == root)

local second_child = sihd.util.Named("second_child", nil)

assert(root:find("toto") == nil)
root:add_child_name("toto", second_child)
assert(root:find("toto") == second_child)

assert(second_child:name() == "second_child")
assert(second_child:parent() == root)
assert(second_child:root() == root)

print(root:tree_desc_str())

local children_tbl = root:children()
assert(children_tbl["child"] ~= nil)
assert(children_tbl["child"].name == "child")
assert(children_tbl["toto"] ~= nil)
assert(children_tbl["toto"].obj:name() == "second_child")

local children_keys = root:children_keys()
assert(children_keys[1] == "child")
assert(children_keys[2] == "toto")

local subparent = sihd.util.Node("subparent", root)
local subchild = sihd.util.Node("subchild", subparent)

assert(root.subparent:parent().subparent.subchild == subchild)
assert(root.doesnotexist == nil)
