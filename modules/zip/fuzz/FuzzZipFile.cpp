#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>

#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/zip/ZipFile.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::sys;
using namespace sihd::util;
using namespace sihd::zip;

constexpr ssize_t max_entries = 16;
// deflate reaches about 1000:1: a small archive may not keep the fuzzer busy for long
constexpr size_t max_entry_bytes = 4 * 1024 * 1024;

void read_hostile_entry(ZipFile & zip, size_t buffer_size)
{
    size_t total = 0;
    while (total < max_entry_bytes)
    {
        const auto read = zip.read_entry();
        if (!read.has_value() || *read == 0)
            return;
        ArrCharView view;
        test::fuzz_check(zip.get_read_data(view), "a read exposes its data");
        test::fuzz_check(view.size() <= buffer_size, "a chunk fits the read buffer");
        total += view.size();
    }
}

void fuzz_hostile_archive(FuzzedDataProvider & provider)
{
    const bool strict_checks = provider.ConsumeBool();
    const size_t buffer_size = provider.ConsumeIntegralInRange<size_t>(1, 4096);
    const std::string content = provider.ConsumeRemainingBytesAsString();

    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "fuzz.zip");
    test::fuzz_check(fs::write_binary(zip_path, content).has_value(), "the archive bytes are written");

    ZipFile zip;
    if (!zip.open(zip_path, true, strict_checks).has_value())
        return;
    test::fuzz_check(zip.set_buffer_size(buffer_size).has_value(), "the read buffer is resized");
    (void)zip.archive_comment();

    const ssize_t entries = std::min(zip.count_original_entries(), max_entries);
    for (ssize_t index = 0; index < entries; ++index)
    {
        if (!zip.load_entry(static_cast<size_t>(index)).has_value())
            continue;
        (void)zip.entry_name();
        (void)zip.entry_comment();
        (void)zip.is_entry_directory();
        read_hostile_entry(zip, buffer_size);
    }
}

void fuzz_round_trip(FuzzedDataProvider & provider)
{
    const size_t buffer_size = provider.ConsumeIntegralInRange<size_t>(1, 4096);
    const std::string name = "entry" + std::to_string(provider.ConsumeIntegralInRange<int>(0, 999)) + ".bin";
    const std::string content = provider.ConsumeRemainingBytesAsString();

    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "fuzz.zip");
    {
        ZipFile zip(zip_path);
        test::fuzz_check(zip.is_open(), "an archive opens for writing");
        test::fuzz_check(zip.add_file(name, ArrCharView(content.data(), content.size())).has_value(),
                         "an entry is added");
        test::fuzz_check(zip.close().has_value(), "an archive closes");
    }

    ZipFile zip(zip_path, true);
    test::fuzz_check(zip.is_open(), "a written archive opens for reading");
    const auto entry = zip.load_entry(name);
    test::fuzz_check(entry.has_value(), "an added entry is found");
    test::fuzz_check(entry->size == content.size(), "the entry keeps its size");
    test::fuzz_check(zip.set_buffer_size(buffer_size).has_value(), "the read buffer is resized");

    std::string result;
    while (true)
    {
        const auto read = zip.read_entry();
        test::fuzz_check(read.has_value(), "an entry is readable");
        if (*read == 0)
            break;
        ArrCharView view;
        test::fuzz_check(zip.get_read_data(view), "a read exposes its data");
        test::fuzz_check(view.size() <= buffer_size, "a chunk fits the read buffer");
        result.append(view.data(), view.size());
    }
    test::fuzz_check(result == content, "the entry content round trips");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_hostile_archive(provider);
    else
        fuzz_round_trip(provider);
    return 0;
}
