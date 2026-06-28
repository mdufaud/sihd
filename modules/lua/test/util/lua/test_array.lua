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

assert(arr_int:at(1) == 23)
arr_int:pop(1)
assert(arr_int:size() == 2)
assert(arr_int:at(0) == 12)
assert(arr_int:at(1) == 34)

arr_int:set(0, 15)
assert(arr_int:at(0) == 15)

arr_int:copy_from({10, 30})
assert(arr_int:at(0) == 10)
assert(arr_int:at(1) == 30)

arr_int:copy_from({20}, 1)
assert(arr_int:at(0) == 10)
assert(arr_int:at(1) == 20)

arr_int:push_back({30, 40, 50})
assert(arr_int:size() == 5)
assert(arr_int:at(0) == 10)
assert(arr_int:at(1) == 20)
assert(arr_int:at(2) == 30)
assert(arr_int:at(3) == 40)
assert(arr_int:at(4) == 50)

-- for i, val in pairs(arr_int) do
--     print("[" .. i .. "] = " .. val)
-- end

local dt = arr_int:data_type()
assert(sihd.util.types.type_size(dt), 4)
assert(sihd.util.types.type_str(dt), "int")
assert(sihd.util.types.from_str("int"), dt)

local arr_int2 = sihd.util.ArrInt.new({1, 2, 3})
assert(arr_int2:str(",") == "1,2,3")

local arr_str = sihd.util.ArrChar.new("hello world")
arr_str:push_back(", how are you ?")
print(arr_str:str())
assert(arr_str:str() == "hello world, how are you ?")

local arr_char = sihd.util.ArrChar.new({"a", "b", "c"})
assert(arr_char:str(",") == "a,b,c")
assert(arr_char:back() == "c")
arr_char:push_back("d")
assert(arr_char:back() == "d")

local arr_bool = sihd.util.ArrBool.new({true, false})
assert(arr_bool:front() == true)
assert(arr_bool:back() == false)
arr_bool:push_back(true)
assert(arr_bool:back() == true)