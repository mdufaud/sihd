local util = sihd.util
local log = util.log

local function must_raise(fn, what)
    local ok, err = pcall(fn)
    assert(ok == false, what .. ": expected an error, got success")
    return err
end

log.info("Array.new rejects invalid argument types")
do
    must_raise(function() util.ArrInt.new(42) end, "ArrInt.new(number)")
    must_raise(function() util.ArrInt.new(true) end, "ArrInt.new(bool)")
    -- a table is valid
    local ok = pcall(function() return util.ArrInt.new({ 1, 2, 3 }) end)
    assert(ok, "ArrInt.new(table) must succeed")
end

log.info("Array:copy_from overflow returns false")
do
    local arr = util.ArrInt.new({ 1, 2 })
    assert(arr:size() == 2, "array should hold two elements")
    local ok, ret = pcall(function() return arr:copy_from({ 10, 20, 30 }) end)
    assert(ok, "copy_from overflow must not raise")
    assert(ret == false, "copy_from of a too-long table must return false")
    -- a fitting copy succeeds
    assert(arr:copy_from({ 7, 8 }) == true, "in-bounds copy_from must return true")
    assert(arr:at(0) == 7 and arr:at(1) == 8, "copy_from must have written the values")
end

log.info("Node/Named lookup corner cases")
do
    local root = util.Node("root", nil)
    local child = util.Node("child", root)

    -- find of a missing key returns nil, not an error
    assert(root:find("nope") == nil, "find of a missing name must return nil")
    -- __index metamethod on a missing key returns nil
    assert(root.doesnotexist == nil, "__index on a missing child must return nil")
    -- find resolves an existing child and the parent up-reference
    assert(root:find("child") ~= nil, "find must resolve an existing child")
    assert(child:find("..") ~= nil, "find('..') must resolve the parent")

    -- __eq: unrelated nodes are not equal, the same node is equal to itself
    local other = util.Node("other", nil)
    assert((root == other) == false, "unrelated nodes must not be equal")
    assert(root:find("child") == child, "the resolved child must equal the original node")
end

log.info("Scheduler:add_task rejects malformed tasks")
do
    local sched = util.Scheduler("bad_add_task", nil)
    must_raise(function() sched:add_task("not a table") end, "add_task(string)")
    must_raise(function() sched:add_task(42) end, "add_task(number)")
    must_raise(function() sched:add_task({}) end, "add_task(table without task fn)")
    must_raise(function() sched:add_task({ task = 5 }) end, "add_task(task not a function)")
    -- a well-formed task is accepted
    local ok = pcall(function()
        sched:add_task({ task = function() return false end, run_in = 0 })
    end)
    assert(ok, "a well-formed add_task must succeed")
end

log.info("all util bad-path assertions passed")
return true
