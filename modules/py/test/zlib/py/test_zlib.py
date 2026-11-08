import gzip
import zlib

import sihd


def channel_bytes(channel):
    out = sihd.util.ArrUByte()
    out.resize(channel.array().size())
    assert(channel.copy_to(out))
    return bytes(out[i] for i in range(out.size()))


payload = "compressed by DevDeflate, read back by the python stdlib"

core = sihd.core.Core("core")
core.add_channel("source", "byte", len(payload))
deflate = sihd.zlib.DevDeflate("deflate", core)
inflate = sihd.zlib.DevInflate("inflate", core)
assert(deflate.set_mode("message"))
assert(deflate.set_level(9))
assert(deflate.set_format("gzip"))
assert(inflate.set_mode("message"))
assert(inflate.set_output_capacity(1024))
assert(not deflate.set_format("zip"))
assert(deflate.add_link("source", "..source"))
assert(inflate.add_link("compressed", "..deflate.source_out"))

assert(core.init())
assert(core.start())

source = core.find_channel("source")
assert(source.write_array(sihd.util.ArrChar(payload)))

compressed = channel_bytes(deflate.find_channel("source_out"))
assert(gzip.decompress(compressed).decode() == payload)
assert(channel_bytes(inflate.find_channel("compressed_out")).decode() == payload)

assert(core.stop())
assert(core.reset())

stream_core = sihd.core.Core("stream_core")
stream_core.add_channel("source", "byte", len(payload))
stream_deflate = sihd.zlib.DevDeflate("deflate", stream_core)
assert(stream_deflate.add_link("source", "..source"))
assert(stream_core.init())
assert(stream_core.start())

decompressor = zlib.decompressobj()
stream_source = stream_core.find_channel("source")
# two different writes: an equal one is not notified by the source
for write in (payload, payload.upper()):
    assert(stream_source.write_array(sihd.util.ArrChar(write)))
    chunk = channel_bytes(stream_deflate.find_channel("source_out"))
    assert(decompressor.decompress(chunk).decode() == write)

assert(stream_core.stop())
assert(stream_core.reset())
