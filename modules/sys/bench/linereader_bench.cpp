#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sihd/sys/File.hpp>
#include <sihd/sys/LineReader.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/str.hpp>

#include <benchmark/benchmark.h>

namespace
{

using namespace sihd::util;
using namespace sihd::sys;

constexpr size_t file_size = 16 * 1024 * 1024;

// empty when the file could not be written
const std::string & lines_path()
{
    static TmpDir dir;
    static const std::string path = [] {
        std::string file_path = fs::combine(dir.path(), "lines.txt");
        return fs::write(file_path, str::generate_random(file_size)).has_value() ? file_path : std::string();
    }();
    return path;
}

// read_all returns the bytes it read, -1 on a read error
template <typename ReadAll>
void bench_read(benchmark::State & state, ReadAll && read_all)
{
    const std::string & path = lines_path();
    if (path.empty())
    {
        state.SkipWithError("cannot write the lines file");
        return;
    }
    for (auto _ : state)
    {
        const int64_t total = read_all(path);
        if (total != static_cast<int64_t>(file_size))
        {
            state.SkipWithError("read " + std::to_string(total) + " bytes of " + std::to_string(file_size));
            break;
        }
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(file_size));
}

void BM_file_read_line(benchmark::State & state)
{
    bench_read(state, [](const std::string & path) -> int64_t {
        File file;
        if (!file.open(path, "rb").has_value())
            return -1;

        char *line = nullptr;
        size_t size = 0;
        int64_t total = 0;
        while (true)
        {
            auto read = file.read_line(&line, &size);
            if (!read.has_value())
            {
                total = -1;
                break;
            }
            if (read.value() == 0)
                break;
            total += static_cast<int64_t>(strlen(line));
        }
        // getdelim allocates line, the caller frees it
        free(line);
        return total;
    });
}

void BM_line_reader(benchmark::State & state)
{
    bench_read(state, [](const std::string & path) -> int64_t {
        LineReader reader(path,
                          {
                              .read_buffsize = 4096,
                              .delimiter_in_line = true,
                          });
        if (!reader.is_open())
            return -1;
        ArrCharView view;
        int64_t total = 0;
        while (true)
        {
            auto next = reader.read_next();
            if (!next.has_value())
                return -1;
            if (next.value() == false)
                break;
            reader.get_read_data(view);
            total += static_cast<int64_t>(view.size());
        }
        return total;
    });
}

void BM_fast_read_line(benchmark::State & state)
{
    bench_read(state, [](const std::string & path) -> int64_t {
        File file(path, "rb");
        if (!file.is_open())
            return -1;
        LineReader::LineReaderOptions options = {
            // no memory of the last read: one byte at a time
            .read_buffsize = 1,
            .delimiter_in_line = true,
        };
        int64_t total = 0;
        while (true)
        {
            auto line = LineReader::fast_read_line(file.file(), options);
            if (!line.has_value())
                break;
            total += static_cast<int64_t>(line.value().size());
        }
        return total;
    });
}

BENCHMARK(BM_file_read_line)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_line_reader)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_fast_read_line)->Unit(benchmark::kMillisecond);

} // namespace
