import sihd

arr_char = sihd.util.ArrChar()
arr_char.resize(1)
arr_char[0] = "a"
assert(arr_char[0] == "a")

arr_char.push_back(["b", "c"])
arr_char.push_back("d")
arr_char.push_back(("e", "f"))
arr_char.push_back(" !")

assert(arr_char.str() == "abcdef !")

arr_int = sihd.util.ArrInt([1, 2, 3])
assert(len(arr_int) == 3)