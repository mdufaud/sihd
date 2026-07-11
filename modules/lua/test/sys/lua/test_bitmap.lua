local sys = sihd.sys

local bmp = sys.Bitmap(4, 4, 32)
assert(bmp:width() == 4)
assert(bmp:height() == 4)
assert(bmp:byte_per_pixel() == 4)
assert(not bmp:empty())

-- Pixel value type
local red = sys.Pixel.rgb(255, 0, 0)
assert(red:red() == 255)
assert(red:green() == 0)
assert(red:blue() == 0)

bmp:fill(red)
local px = bmp:get(0, 0)
assert(px:red() == 255)
assert(px:green() == 0)
assert(px:blue() == 0)

-- set a single pixel then read it back
bmp:set(1, 1, sys.Pixel.rgb(0, 0, 255))
assert(bmp:get(1, 1):blue() == 255)
assert(bmp:is_accessible(1, 1))
assert(not bmp:is_accessible(10, 10))

-- in-memory BMP round-trip preserves geometry + filled color
local data = bmp:to_bmp_data()
assert(#data > 0)
local bmp2 = sys.Bitmap()
assert(bmp2:read_bmp_data(data))
assert(bmp2:width() == 4)
assert(bmp2:height() == 4)
assert(bmp2:get(0, 0):red() == 255)

-- screenshot binding is present (actual capture needs a display)
assert(type(sys.screenshot.supported()) == "boolean")

-- FileMutex: advisory lock round-trip on a temp file
local fm = sys.FileMutex("/tmp/sihd_lua_filemutex_test.lock", true)
assert(fm:try_lock())
fm:unlock()
assert(fm:try_lock_for(100))
fm:unlock()
assert(fm:try_lock_shared())
fm:unlock_shared()
