local arr_int = sihd.util.ArrInt()

arr_int:push_back(12)
arr_int:push_back(23)
arr_int:push_back(34)

assert(arr_int:front() == 12)
assert(arr_int:back() == 34)

assert(arr_int:at(0) == 12)
assert(arr_int:at(1) == 23)
assert(arr_int:at(2) == 34)

assert(arr_int:size() == 3)
assert(#arr_int == 3)

assert(arr_int:pop(1) == 23)
assert(arr_int:size() == 2)
assert(arr_int:at(0) == 12)
assert(arr_int:at(1) == 34)

arr_int:set(0, 15)
assert(arr_int:at(0) == 15)

-- for i, val in pairs(arr_int) do
--     print("[" .. i .. "] = " .. val)
-- end

local dt = arr_int:data_type()
assert(sihd.util.types.type_size(dt), 4)
assert(sihd.util.types.type_to_string(dt), "int")
assert(sihd.util.types.string_to_type("int"), dt)

local arr_int2 = sihd.util.ArrInt.from({1, 2, 3})
assert(arr_int2:to_string(",") == "1,2,3")

local arr_str = sihd.util.ArrStr.from("hello world")
arr_str:push_back(", how are you ?")
assert(arr_str:str() == "hello world, how are you ?")

local arr_str2 = sihd.util.ArrStr.from({'a', 'b', 'c'})
assert(arr_str2:str() == "abc")