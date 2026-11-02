#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <memory>
#include <string>
#include <vector>

#include <sihd/core/Channel.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Slice.hpp>
#include <sihd/util/Value.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::util;
using sihd::core::Channel;

class Counter: public IHandler<Channel *>
{
    public:
        void handle(Channel *) override { ++count; }

        int count = 0;
};

std::vector<int8_t> consume_bytes(FuzzedDataProvider & provider, size_t min_size, size_t max_size)
{
    std::vector<int8_t> bytes(provider.ConsumeIntegralInRange<size_t>(min_size, max_size));
    for (int8_t & byte : bytes)
        byte = provider.ConsumeIntegralInRange<int8_t>(0, 3);
    return bytes;
}

bool same_content(const Channel & channel, const std::vector<int8_t> & model)
{
    return channel.byte_size() == model.size() && std::equal(model.begin(), model.end(), channel.data());
}

Slice consume_slice(FuzzedDataProvider & provider)
{
    return {provider.ConsumeIntegralInRange<ssize_t>(-100, 100), provider.ConsumeIntegralInRange<ssize_t>(-100, 100)};
}

void fuzz_copy_to(FuzzedDataProvider & provider)
{
    const size_t count = provider.ConsumeIntegralInRange<size_t>(1, 64);
    ArrInt src;
    src.resize(count);
    for (size_t i = 0; i < count; ++i)
        src[i] = provider.ConsumeIntegral<int>();
    Channel channel("chan", "int", count);
    test::fuzz_check(channel.write(src), "a channel takes an array of its size");

    const Slice slice = consume_slice(provider);
    const Slice::Range range = slice.resolve(count);
    ArrInt dst;
    dst.resize(range.size());
    test::fuzz_check(channel.copy_to(dst, slice) == !range.empty(), "copy_to fails exactly on an empty slice");
    for (size_t i = 0; i < range.size(); ++i)
        test::fuzz_check(dst[i] == src[range.from + i], "copy_to copies the resolved elements");
}

void fuzz_copy_to_bytes(FuzzedDataProvider & provider)
{
    const size_t count = provider.ConsumeIntegralInRange<size_t>(1, 64);
    Array<uint8_t> src;
    src.resize(count);
    for (size_t i = 0; i < count; ++i)
        src[i] = provider.ConsumeIntegral<uint8_t>();
    Channel channel("chan", "ubyte", count);
    test::fuzz_check(channel.write(src), "a channel takes an array of its size");

    const Slice slice = consume_slice(provider);
    const Slice::Range range = slice.resolve(count);
    Array<uint8_t> dst;
    dst.resize(range.size());
    test::fuzz_check(channel.copy_to_bytes(dst, slice) == !range.empty(),
                     "copy_to_bytes fails exactly on an empty slice");
    for (size_t i = 0; i < range.size(); ++i)
        test::fuzz_check(dst[i] == src[range.from + i], "copy_to_bytes copies the resolved bytes");
}
// bytes are written at an offset: it must fit the channel, and only a changed content notifies
void fuzz_write_bytes(FuzzedDataProvider & provider)
{
    const size_t count = provider.ConsumeIntegralInRange<size_t>(1, 8);
    const size_t byte_size = count * sizeof(int);
    const bool on_change = provider.ConsumeBool();
    Channel channel("chan", "int", count);
    Counter counter;
    channel.add_observer(&counter);
    channel.set_write_on_change(on_change);
    std::vector<int8_t> model(byte_size, 0);

    for (size_t ops = provider.ConsumeIntegralInRange<size_t>(1, 24); ops > 0; --ops)
    {
        const size_t offset = provider.ConsumeIntegralInRange<size_t>(0, byte_size + 4);
        const std::vector<int8_t> bytes = consume_bytes(provider, 1, byte_size + 4);
        const bool fits = offset + bytes.size() <= byte_size;
        const bool changes = fits
                             && !std::equal(bytes.begin(), bytes.end(), model.begin() + static_cast<ptrdiff_t>(offset));
        const int notified_before = counter.count;

        const bool written = channel.write(ArrByteView(bytes.data(), bytes.size()), offset);
        test::fuzz_check(written == fits, "a write is accepted exactly when it fits the channel");
        if (fits)
            std::copy(bytes.begin(), bytes.end(), model.begin() + static_cast<ptrdiff_t>(offset));
        test::fuzz_check(same_content(channel, model), "a write changes only the bytes it targets");
        test::fuzz_check(counter.count == notified_before + ((on_change ? changes : fits) ? 1 : 0),
                         "observers are notified by a write, only a changing one when writing on change");
    }
}

void fuzz_resizable(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(1, 8);
    const size_t capacity = provider.ConsumeIntegralInRange<size_t>(1, 12);
    const std::string conf = "name=chan;type=ubyte;size=" + std::to_string(size)
                             + ";capacity=" + std::to_string(capacity);
    const std::unique_ptr<Channel> channel(Channel::build(conf));
    test::fuzz_check(channel != nullptr, "a channel is built from its configuration");
    test::fuzz_check(channel->size() == size, "the built channel has the configured size");
    const bool resizable = capacity > size;
    test::fuzz_check(channel->resizable() == resizable, "a channel is resizable when its capacity exceeds its size");
    test::fuzz_check(!resizable || channel->capacity() >= capacity, "a resizable channel reserves its capacity");
    const size_t limit = resizable ? channel->capacity() : size;

    std::vector<int8_t> model(size, 0);
    for (size_t ops = provider.ConsumeIntegralInRange<size_t>(1, 16); ops > 0; --ops)
    {
        const bool at_offset = provider.ConsumeBool();
        const size_t offset = at_offset ? provider.ConsumeIntegralInRange<size_t>(0, model.size()) : 0;
        const std::vector<int8_t> bytes = consume_bytes(provider, 1, limit + 2);
        const size_t needed = offset + bytes.size();
        const ArrByteView view(bytes.data(), bytes.size());

        bool fits = needed <= limit;
        if (!resizable)
            fits = needed <= model.size();
        const bool written = at_offset ? channel->write(view, offset) : channel->write(view);
        test::fuzz_check(written == fits, "a write fails exactly when it exceeds the size or the capacity");
        if (fits)
        {
            if (resizable && (!at_offset || needed > model.size()))
                model.resize(at_offset ? needed : bytes.size());
            std::copy(bytes.begin(), bytes.end(), model.begin() + static_cast<ptrdiff_t>(offset));
        }
        test::fuzz_check(same_content(*channel, model), "a write resizes the channel like a resizable one should");
    }
}

// a write that needs no resize behaves like the write of a fixed size channel
void fuzz_resizable_same_size(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(1, 8);
    const std::string conf = "name=chan;type=ubyte;size=" + std::to_string(size)
                             + ";capacity=" + std::to_string(size + 2);
    const std::unique_ptr<Channel> channel(Channel::build(conf));
    test::fuzz_check(channel != nullptr, "a channel is built from its configuration");
    const std::vector<int8_t> bytes = consume_bytes(provider, size, size);
    const ArrByteView view(bytes.data(), bytes.size());
    test::fuzz_check(channel->write(view), "a write of the channel size succeeds");

    Counter counter;
    channel->add_observer(&counter);
    channel->set_write_on_change(true);
    test::fuzz_check(channel->write(view, size_t {0}), "an unchanged write succeeds");
    test::fuzz_check(counter.count == 0, "an unchanged write that fits does not notify");

    (void)channel->write(ArrByteView(bytes.data(), 0));
    test::fuzz_check(channel->size() == size && same_content(*channel, bytes), "an empty write does not resize");
}

void fuzz_value_at(FuzzedDataProvider & provider)
{
    const size_t count = provider.ConsumeIntegralInRange<size_t>(1, 8);
    Channel as_int("int", "int", count);
    Channel as_uchar("uchar", "ubyte", count);
    Channel as_double("double", "double", count);
    std::vector<int> values(count);
    for (size_t i = 0; i < count; ++i)
    {
        values[i] = provider.ConsumeIntegralInRange<int>(0, 200);
        test::fuzz_check(as_int.write(i, values[i]), "an int is written");
        test::fuzz_check(as_uchar.write(i, static_cast<uint8_t>(values[i])), "a byte is written");
        test::fuzz_check(as_double.write(i, values[i] / 4.0), "a double is written");
    }

    const size_t idx = provider.ConsumeIntegralInRange<size_t>(0, count + 2);
    if (idx >= count)
    {
        test::fuzz_check(
            as_int.value_at(idx).empty() && as_uchar.value_at(idx).empty() && as_double.value_at(idx).empty(),
            "a value out of range is empty");
        return;
    }
    test::fuzz_check(as_int.value_at(idx) == values[idx] && as_uchar.value_at(idx) == values[idx],
                     "an integer element is read as its value");
    test::fuzz_check(as_double.value_at(idx) == values[idx] / 4.0 && as_double.value_at(idx).is_float(),
                     "a double element is read as a float value");
    test::fuzz_check(as_int.read<int>(idx) == values[idx] && as_double.read<double>(idx) == values[idx] / 4.0,
                     "read returns the written element");
}

void fuzz_build(FuzzedDataProvider & provider)
{
    const bool with_name = provider.ConsumeBool();
    const bool with_type = provider.ConsumeBool();
    const bool with_size = provider.ConsumeBool();
    const bool good_size = provider.ConsumeBool();
    const size_t size = provider.ConsumeIntegralInRange<size_t>(0, 64);

    std::string conf = "x=1";
    if (with_name)
        conf += ";name=chan";
    if (with_type)
        conf += ";type=short";
    if (with_size)
        conf += ";size=" + (good_size ? std::to_string(size) : std::string("many"));

    const std::unique_ptr<Channel> channel(Channel::build(conf));
    const bool expected = with_name && with_type && with_size && good_size;
    test::fuzz_check((channel != nullptr) == expected,
                     "a channel is built only from a name, a type and a numeric size");
    if (channel)
        test::fuzz_check(
            channel->size() == size && channel->byte_size() == size * sizeof(int16_t) && channel->name() == "chan",
            "a built channel has the configured name, type and size");
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 6))
    {
        case 0:
            fuzz_copy_to(provider);
            break;
        case 1:
            fuzz_copy_to_bytes(provider);
            break;
        case 2:
            fuzz_write_bytes(provider);
            break;
        case 3:
            fuzz_resizable(provider);
            break;
        case 4:
            fuzz_value_at(provider);
            break;
        case 5:
            fuzz_resizable_same_size(provider);
            break;
        default:
            fuzz_build(provider);
            break;
    }
    return 0;
}
