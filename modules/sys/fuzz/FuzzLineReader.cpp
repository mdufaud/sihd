#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fuzz_helper.hpp>
#include <sstream>
#include <string>
#include <vector>

#include <sihd/sys/LineReader.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::sys;
using namespace sihd::util;

namespace
{
FILE *open_memory(std::vector<char> & input)
{
    FILE *stream = fmemopen(input.data(), input.size(), "r");
    test::fuzz_check(stream != nullptr, "an in-memory stream opens");
    return stream;
}

std::vector<std::string> split_lines(const std::vector<char> & input, char delimiter, bool keep_delimiter)
{
    std::istringstream stream(std::string(input.begin(), input.end()));
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line, delimiter))
    {
        if (keep_delimiter && !stream.eof())
            line += delimiter;
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> read_lines(std::vector<char> & input, const LineReader::LineReaderOptions & options)
{
    LineReader reader(options);
    test::fuzz_check(reader.set_stream(open_memory(input), true).has_value(), "the reader takes the stream");

    std::vector<std::string> lines;
    while (true)
    {
        const Result<bool> next = reader.read_next();
        test::fuzz_check(next.has_value(), "reading an in-memory stream does not fail");
        if (!*next)
            break;
        ArrCharView view;
        test::fuzz_check(reader.get_read_data(view), "a line is readable");
        lines.emplace_back(view.data(), view.size());
        test::fuzz_check(lines.size() <= input.size(), "every line consumes a byte");
    }

    const Result<bool> after_end = reader.read_next();
    test::fuzz_check(after_end.has_value() && !*after_end, "the end of the stream stays the end");
    test::fuzz_check(reader.close(), "the reader closes the stream");
    return lines;
}

std::vector<std::string> read_fast_lines(std::vector<char> & input, LineReader::LineReaderOptions options)
{
    // no memory between two calls: only a one byte read buffer leaves the next line in the stream
    options.read_buffsize = 1;
    FILE *stream = open_memory(input);

    std::vector<std::string> lines;
    while (true)
    {
        const Result<std::string> line = LineReader::fast_read_line(stream, options);
        if (!line.has_value())
            break;
        lines.push_back(*line);
        test::fuzz_check(lines.size() <= input.size(), "every line consumes a byte");
    }
    std::fclose(stream);
    return lines;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    LineReader::LineReaderOptions options;
    options.read_buffsize = provider.ConsumeIntegralInRange<size_t>(1, 32);
    options.line_buffsize = provider.ConsumeIntegralInRange<size_t>(1, 32);
    options.delimiter_in_line = provider.ConsumeBool();
    options.delimiter = provider.ConsumeBool() ? '\n' : provider.ConsumeIntegral<uint8_t>();
    // exact size heap buffer: a read past the end of the stream is an out of bounds access
    std::vector<char> input = provider.ConsumeRemainingBytes<char>();
    if (input.empty())
        return 0;

    const std::vector<std::string> expected = split_lines(input,
                                                          static_cast<char>(options.delimiter),
                                                          options.delimiter_in_line);
    test::fuzz_check(read_lines(input, options) == expected, "the lines are the std::getline split of the input");
    test::fuzz_check(read_fast_lines(input, options) == expected, "fast_read_line reads the same lines");
    return 0;
}
