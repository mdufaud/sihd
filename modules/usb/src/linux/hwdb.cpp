#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <fnmatch.h>

# include <array>
# include <cstdint>
# include <map>
# include <string>
# include <string_view>

# include <fmt/format.h>

# include <sihd/sys/fs.hpp>
# include <sihd/usb/linux/hwdb.hpp>
# include <sihd/util/ZStringView.hpp>

using namespace sihd::util;
using namespace sihd::sys;
using enum sihd::util::ErrorCode;

namespace sihd::usb::linux_::hwdb
{

namespace
{

// search order of systemd's libudev
constexpr std::array<ZStringView, 4> database_paths = {
    "/etc/systemd/hwdb/hwdb.bin",
    "/etc/udev/hwdb.bin",
    "/usr/lib/systemd/hwdb/hwdb.bin",
    "/usr/lib/udev/hwdb.bin",
};

constexpr std::string_view vendor_key = "ID_VENDOR_FROM_DATABASE";
constexpr std::string_view model_key = "ID_MODEL_FROM_DATABASE";
constexpr std::string_view class_key = "ID_USB_CLASS_FROM_DATABASE";
constexpr std::string_view subclass_key = "ID_USB_SUBCLASS_FROM_DATABASE";
constexpr std::string_view protocol_key = "ID_USB_PROTOCOL_FROM_DATABASE";

constexpr std::string_view signature = "KSLPHHRH";
constexpr uint64_t header_size = 80;
constexpr uint64_t node_size_min = 24;
constexpr uint64_t child_entry_size_min = 16;
constexpr uint64_t value_entry_size_min = 16;
constexpr uint64_t value_entry2_size = 32;
constexpr size_t key_length_max = 4096;

constexpr uint64_t header_file_size_off = 16;
constexpr uint64_t header_node_size_off = 32;
constexpr uint64_t header_child_entry_size_off = 40;
constexpr uint64_t header_value_entry_size_off = 48;
constexpr uint64_t header_root_off = 56;
constexpr uint64_t node_children_count_off = 8;
constexpr uint64_t node_values_count_off = 16;
constexpr uint64_t child_entry_node_off = 8;
constexpr uint64_t value_entry_value_off = 8;
constexpr uint64_t value_filename_off = 16;
constexpr uint64_t value_line_off = 24;
constexpr uint64_t value_priority_off = 28;

struct Property
{
        std::string value;
        uint64_t filename_off = 0;
        uint32_t line_number = 0;
        uint16_t file_priority = 0;
};

// on-disk trie of systemd-hwdb: little-endian, every offset is relative to the file start
class Trie
{
    public:
        explicit Trie(std::string data): _data(std::move(data)) {}

        bool valid()
        {
            if (_data.size() < header_size || !_data.starts_with(signature)
                || read(header_file_size_off, 8) != _data.size())
                return false;
            _node_size = read(header_node_size_off, 8);
            _child_entry_size = read(header_child_entry_size_off, 8);
            _value_entry_size = read(header_value_entry_size_off, 8);
            _root = read(header_root_off, 8);
            return _node_size >= node_size_min && _child_entry_size >= child_entry_size_min
                   && _value_entry_size >= value_entry_size_min && _root != 0;
        }

        bool corrupt() const { return _corrupt; }

        std::string lookup(const std::string & modalias, std::string_view key)
        {
            _properties.clear();
            this->search(modalias);
            if (auto it = _properties.find(key); it != _properties.end())
                return it->second.value;
            return {};
        }

    private:
        uint64_t read(uint64_t off, uint64_t bytes)
        {
            if (off > _data.size() || bytes > _data.size() - off)
            {
                _corrupt = true;
                return 0;
            }
            uint64_t value = 0;
            for (uint64_t i = bytes; i > 0; --i)
                value = (value << 8) | static_cast<uint8_t>(_data[off + i - 1]);
            return value;
        }

        std::string_view string(uint64_t off)
        {
            const size_t end = off < _data.size() ? _data.find('\0', off) : std::string::npos;
            if (end == std::string::npos)
            {
                _corrupt = true;
                return {};
            }
            return std::string_view(_data).substr(off, end - off);
        }

        uint64_t children_count(uint64_t node) { return read(node + node_children_count_off, 1); }

        uint64_t child_entry(uint64_t node, uint64_t index) { return node + _node_size + index * _child_entry_size; }

        uint64_t child(uint64_t node, char c)
        {
            const uint64_t count = this->children_count(node);
            for (uint64_t i = 0; i < count && !_corrupt; ++i)
            {
                const uint64_t entry = this->child_entry(node, i);
                if (this->read(entry, 1) == static_cast<uint8_t>(c))
                    return this->read(entry + child_entry_node_off, 8);
            }
            return 0;
        }

        void add_values(uint64_t node)
        {
            const uint64_t count = this->read(node + node_values_count_off, 8);
            const uint64_t values = node + _node_size + this->children_count(node) * _child_entry_size;
            for (uint64_t i = 0; i < count && !_corrupt; ++i)
            {
                const uint64_t entry = values + i * _value_entry_size;
                const std::string_view key = this->string(this->read(entry, 8));
                // keys without the leading space are reserved for future extensions
                if (key.empty() || key[0] != ' ')
                    continue;

                Property property;
                property.value = this->string(this->read(entry + value_entry_value_off, 8));
                if (_value_entry_size >= value_entry2_size)
                {
                    property.filename_off = this->read(entry + value_filename_off, 8);
                    property.line_number = static_cast<uint32_t>(this->read(entry + value_line_off, 4));
                    property.file_priority = static_cast<uint16_t>(this->read(entry + value_priority_off, 2));
                }

                auto [it, inserted] = _properties.try_emplace(std::string(key.substr(1)), property);
                if (!inserted && !this->is_lower(property, it->second))
                    it->second = std::move(property);
            }
        }

        // v2 files carry no priority: filenames are stored in priority order instead
        bool is_lower(const Property & property, const Property & old) const
        {
            if (_value_entry_size < value_entry2_size)
                return false;
            if (property.file_priority == 0)
                return property.filename_off < old.filename_off
                       || (property.filename_off == old.filename_off && property.line_number < old.line_number);
            return property.file_priority < old.file_priority
                   || (property.file_priority == old.file_priority && property.line_number < old.line_number);
        }

        void match(uint64_t node, size_t prefix_pos, std::string & pattern, const std::string & search)
        {
            const std::string_view prefix = this->string(this->read(node, 8));
            if (_corrupt || prefix_pos > prefix.size())
            {
                _corrupt = true;
                return;
            }
            const std::string_view tail = prefix.substr(prefix_pos);
            if (pattern.size() + tail.size() >= key_length_max)
            {
                _corrupt = true;
                return;
            }
            pattern.append(tail);

            const uint64_t count = this->children_count(node);
            for (uint64_t i = 0; i < count && !_corrupt; ++i)
            {
                const uint64_t entry = this->child_entry(node, i);
                pattern.push_back(static_cast<char>(this->read(entry, 1)));
                this->match(this->read(entry + child_entry_node_off, 8), 0, pattern, search);
                pattern.pop_back();
            }

            if (this->read(node + node_values_count_off, 8) > 0 && ::fnmatch(pattern.c_str(), search.c_str(), 0) == 0)
                this->add_values(node);

            pattern.resize(pattern.size() - tail.size());
        }

        bool follow_prefix(uint64_t node, const std::string & modalias, size_t & pos, std::string & pattern)
        {
            const uint64_t prefix_off = this->read(node, 8);
            if (prefix_off == 0)
                return true;
            const std::string_view prefix = this->string(prefix_off);
            for (size_t offset = 0; offset < prefix.size(); ++offset)
            {
                const char prefix_char = prefix[offset];
                if (prefix_char == '*' || prefix_char == '?' || prefix_char == '[')
                {
                    this->match(node, offset, pattern, modalias.substr(pos + offset));
                    return false;
                }
                if (pos + offset >= modalias.size() || prefix_char != modalias[pos + offset])
                    return false;
            }
            pos += prefix.size();
            return true;
        }

        void search(const std::string & modalias)
        {
            std::string pattern;
            uint64_t node = _root;
            size_t pos = 0;
            while (node != 0 && !_corrupt)
            {
                if (!this->follow_prefix(node, modalias, pos, pattern))
                    return;

                for (const char wildcard : {'*', '?', '['})
                {
                    if (const uint64_t wildcard_child = this->child(node, wildcard); wildcard_child != 0)
                    {
                        pattern.push_back(wildcard);
                        this->match(wildcard_child, 0, pattern, modalias.substr(pos));
                        pattern.pop_back();
                    }
                }

                if (pos == modalias.size())
                {
                    this->add_values(node);
                    return;
                }
                node = this->child(node, modalias[pos]);
                ++pos;
            }
        }

        std::string _data;
        uint64_t _node_size = 0;
        uint64_t _child_entry_size = 0;
        uint64_t _value_entry_size = 0;
        uint64_t _root = 0;
        bool _corrupt = false;
        std::map<std::string, Property, std::less<>> _properties;
};

Result<Trie> load_trie()
{
    for (const ZStringView path : database_paths)
    {
        auto data = fs::read_all(path, true);
        if (!data.has_value() && data.error().code == not_found)
            continue;
        SIHD_UNEXPECTED_RETURN(data);
        Trie trie(std::move(*data));
        if (!trie.valid())
            return std::unexpected(Error(io_error, "unrecognized usb hardware database format: {}", path));
        return trie;
    }
    return std::unexpected(Error(not_found, "no usb hardware database"));
}

} // namespace

Result<void> enrich(std::vector<UsbDevice> & devices)
{
    auto trie = load_trie();
    SIHD_UNEXPECTED_RETURN(trie);

    for (UsbDevice & device : devices)
    {
        device.vendor_name = trie->lookup(fmt::format("usb:v{:04X}*", device.vendor_id), vendor_key);
        device.product_name = trie->lookup(fmt::format("usb:v{:04X}p{:04X}*", device.vendor_id, device.product_id),
                                           model_key);
        device.class_name = trie->lookup(fmt::format("usb:v*p*d*dc{:02X}*", device.device_class), class_key);
        device.subclass_name = trie->lookup(
            fmt::format("usb:v*p*d*dc{:02X}dsc{:02X}*", device.device_class, device.device_subclass),
            subclass_key);
        device.protocol_name = trie->lookup(fmt::format("usb:v*p*d*dc{:02X}dsc{:02X}dp{:02X}*",
                                                        device.device_class,
                                                        device.device_subclass,
                                                        device.device_protocol),
                                            protocol_key);
    }
    if (trie->corrupt())
        return std::unexpected(Error(io_error, "corrupt usb hardware database"));
    return {};
}

} // namespace sihd::usb::linux_::hwdb

#endif
