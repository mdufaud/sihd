#include <algorithm>
#include <array>
#include <cmath>
#include <deque>

#include <sihd/tui/channel_view.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Value.hpp>

namespace sihd::tui
{

using namespace ftxui;
using namespace sihd::util;
using namespace sihd::core;

namespace
{

std::string bars_of(const std::deque<double> & samples)
{
    static const std::array<const char *, 8> bars = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    if (samples.empty())
        return {};
    const auto [low, high] = std::minmax_element(samples.begin(), samples.end());
    std::string line;
    for (double sample : samples)
    {
        const double ratio = *high > *low ? (sample - *low) / (*high - *low) : 0.0;
        line += bars[static_cast<size_t>(ratio * 7.0 + 0.5)];
    }
    return line;
}

std::string bytes_str(const uint8_t *bytes, size_t byte_size, Type type)
{
    if (type == TYPE_CHAR)
    {
        const char *chars = reinterpret_cast<const char *>(bytes);
        return std::string(chars, std::find(chars, chars + byte_size, '\0'));
    }
    const size_t element_size = type::size(type);
    std::string str;
    for (size_t offset = 0; element_size > 0 && offset + element_size <= byte_size; offset += element_size)
    {
        if (str.empty() == false)
            str += ' ';
        str += Value(bytes + offset, type).str();
    }
    return str;
}

} // namespace

std::string channel_str(const Channel & channel)
{
    ArrByte bytes;
    if (channel.copy_resized_to(bytes) == false)
        return {};
    return bytes_str(bytes.buf(), bytes.byte_size(), channel.data_type());
}

std::string array_str(const IArray & array)
{
    return bytes_str(array.buf(), array.byte_size(), array.data_type());
}

Element channel_text(const Channel & channel)
{
    return text(channel_str(channel));
}

Element channel_gauge(const Channel & channel, double min, double max)
{
    const Value value = channel.value_at(0);
    const double ratio = value.empty() || max <= min ? 0.0 : (value.to_double() - min) / (max - min);
    return gauge(static_cast<float>(std::clamp(ratio, 0.0, 1.0)));
}

Component ChannelSparkline(const Channel & channel, size_t samples)
{
    auto history = std::make_shared<std::deque<double>>();
    return Renderer([&channel, samples, history] {
        const Value value = channel.value_at(0);
        // a NaN or infinite sample has no bar and would poison the low and high of the history
        if (value.empty() == false && std::isfinite(value.to_double()))
        {
            history->push_back(value.to_double());
            if (history->size() > samples)
                history->pop_front();
        }
        return text(bars_of(*history));
    });
}

} // namespace sihd::tui
