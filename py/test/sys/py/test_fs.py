import os
import sihd

fs = sihd.sys.fs

# test sihd.dir attribute
assert(isinstance(sihd.dir, str))
assert(len(sihd.dir) > 0)

# test cwd
cwd = fs.cwd()
assert(isinstance(cwd, str))
assert(len(cwd) > 0)

# test executable_path
exe = fs.executable_path()
assert(isinstance(exe, str))
assert(len(exe) > 0)

# test home_path
home = fs.home_path()
assert(isinstance(home, str))
assert(len(home) > 0)
assert(fs.is_dir(home))

# test is_absolute
assert(fs.is_absolute("/tmp"))
assert(not fs.is_absolute("relative/path"))

# test combine
combined = fs.combine("/tmp", "test")
assert(combined == "/tmp/test")

# test parent
assert(fs.parent("/tmp/test") == "/tmp")

# test filename
assert(fs.filename("/tmp/test.txt") == "test.txt")

# test extension
assert(fs.extension("/tmp/test.txt") == "txt")

# test normalize
assert(fs.normalize("/tmp/../tmp/test") == "/tmp/test")

# test make/remove directory
test_dir = fs.combine(cwd, "py_test_fs_dir")
if fs.exists(test_dir):
    fs.remove_directory(test_dir)
assert(fs.make_directory(test_dir))
assert(fs.is_dir(test_dir))
assert(fs.exists(test_dir))
assert(not fs.is_file(test_dir))
assert(fs.remove_directory(test_dir))
assert(not fs.exists(test_dir))

# test make_directories
nested = fs.combine(cwd, "py_test_fs_dir/nested/deep")
assert(fs.make_directories(nested))
assert(fs.is_dir(nested))
assert(fs.remove_directories(fs.combine(cwd, "py_test_fs_dir")))

# test children
children = fs.children(cwd)
assert(isinstance(children, list))
assert(len(children) > 0)

# test file operations via temp file
import tempfile
with tempfile.NamedTemporaryFile(delete=False) as f:
    f.write(b"hello world")
    tmp_path = f.name

assert(fs.exists(tmp_path))
assert(fs.is_file(tmp_path))
assert(fs.file_size(tmp_path) == 11)
assert(fs.remove_file(tmp_path))
assert(not fs.exists(tmp_path))

print("fs tests passed")
