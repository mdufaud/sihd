# zlib

Deflate compression of byte streams on zlib. `Deflater` and `Inflater` compress and decompress buffers, and `DevDeflate` and `DevInflate` do the same on channels: every linked channel `NAME` is compressed or decompressed on `NAME_out`.

| | |
|---|---|
| Depends on | `core` |
| External libraries | zlib |
| Platforms | all |
| Build | `make m=zlib` |
| Header | `#include <sihd/zlib/DevDeflate.hpp>` (also `DevInflate.hpp`, `Deflater.hpp`, `Inflater.hpp`, `zlib.hpp`) |

## Capabilities

### Modes

Both classes and both devices work in one of two modes:

| Mode | zlib flush | Output of one write | Transport |
|---|---|---|---|
| `stream` | `Z_SYNC_FLUSH` | decodable at once, but depends on every previous output of the stream | ordered and lossless: TCP, pipe, file |
| `message` | `Z_FINISH` | a complete stream, decoded alone | any, UDP included: a lost write loses only itself |

`message` compresses less on small writes: each one restarts the dictionary and carries a header and a trailer.

In `stream` mode, a missing or damaged write breaks every following one until both ends restart. A device restart (`stop` then `start`) starts a new stream, so restart the deflating and inflating ends together.

### Deflater

- `Deflater(format = Format::zlib, level = default_level)`: `Format::zlib` or `Format::gzip` wrapper; level `min_level` (0, stored) to `max_level` (9, best), `default_level` is 6, any other level throws `std::invalid_argument`.
- `deflate_stream(view)`: compresses a chunk of the stream, returns `Result<ArrByte>`.
- `deflate_message(view)`: ends the stream with this input and returns it, a complete stream when no `deflate_stream` call preceded; the next call starts a new stream.
- `output_bound(input_size)`: largest output of one call for `input_size` bytes.

### Inflater

- `Inflater(max_output_size)`: detects a zlib or a gzip header.
- `inflate_stream(view)`: continues the stream; a stream end restarts on the next header, so concatenated streams decode in one call.
- `inflate_message(view)`: the input must hold exactly one complete stream.
- Errors: corrupted data, a truncated message or bytes after the end of a message give `invalid_argument`; an output larger than `max_output_size` gives `overflow`. The state restarts after an error and after each message.

### Devices

- `DevDeflate`: confs `mode` (`stream` default, `message`), `level` (`0` to `9`, default `6`), `format` (`zlib` default, `gzip`). `NAME_out` is a resizable byte channel sized for the worst compression of the input capacity.
- `DevInflate`: confs `mode` (`stream` default, `message`), `output_capacity` (bytes, default `65536`). `NAME_out` is a resizable byte channel of `output_capacity`; a write that decompresses to more is dropped and logged.
- Both devices work on the thread that writes the input: `NAME_out` is written before the input write returns. A failed write is logged and produces no output.

## Example

Adapted from `modules/zlib/test/TestDevDeflate.cpp`.

```cpp
#include <string_view>

#include <sihd/core/Core.hpp>
#include <sihd/zlib/DevDeflate.hpp>
#include <sihd/zlib/DevInflate.hpp>

void compress_through_channels()
{
    sihd::core::Core core;
    sihd::core::Channel *source = core.add_channel_resizable("source", sihd::util::TYPE_BYTE, 0, 4096);
    auto *deflate = core.add_child<sihd::zlib::DevDeflate>("deflate");
    auto *inflate = core.add_child<sihd::zlib::DevInflate>("inflate");
    (void)deflate->set_conf_str("mode", "message");
    (void)inflate->set_conf_str("mode", "message");
    deflate->add_link("source", "..source");
    inflate->add_link("compressed", "..deflate.source_out");
    if (!core.init() || !core.start())
        return;

    source->write(sihd::util::ArrByteView(std::string_view("bytes to squeeze")));
    // deflate.source_out holds a zlib stream, inflate.compressed_out holds "bytes to squeeze"

    core.stop();
}
```

## Scripting

Exposed to Lua (requires `lua=1`, module `sihd.zlib`): `Deflater` (`Deflater.Format`), `Inflater`, `DevDeflate`, `DevInflate`. Failures raise a bound `Error`.

Exposed to Python (requires `py=1`, module `sihd.zlib`): `DevDeflate`, `DevInflate`. The standard `zlib` and `gzip` modules decode their output and cover `Deflater` and `Inflater`.
