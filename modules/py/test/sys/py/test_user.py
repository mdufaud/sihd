import sihd

user_mod = sihd.sys.user

# test is_root
is_root = user_mod.is_root()
assert(isinstance(is_root, bool))

# test name
name = user_mod.name()
assert(isinstance(name, str))
assert(len(name) > 0)

print("user tests passed")
