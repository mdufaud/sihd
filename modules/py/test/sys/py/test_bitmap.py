import sihd

bmp = sihd.sys.Bitmap(4, 4, 32)
assert bmp.width() == 4
assert bmp.height() == 4
assert bmp.byte_per_pixel() == 4
assert not bmp.empty()

# Pixel value type
red = sihd.sys.Pixel.rgb(255, 0, 0)
assert red.red == 255
assert red.green == 0
assert red.blue == 0

bmp.fill(red)
assert bmp.get(0, 0).red == 255

bmp.set(1, 1, sihd.sys.Pixel.rgb(0, 0, 255))
assert bmp.get(1, 1).blue == 255
assert bmp.is_accessible(1, 1)
assert not bmp.is_accessible(10, 10)

# in-memory BMP round-trip
data = bmp.to_bmp_data()
assert len(data) > 0
bmp2 = sihd.sys.Bitmap()
assert bmp2.read_bmp_data(data)
assert bmp2.width() == 4
assert bmp2.height() == 4
assert bmp2.get(0, 0).red == 255

# screenshot binding present (capture needs a display)
assert isinstance(sihd.sys.screenshot.supported, bool)

print("bitmap tests passed")
