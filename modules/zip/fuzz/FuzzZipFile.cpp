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

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
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
    return 0;
}
