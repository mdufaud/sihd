#include <cstddef>
#include <cstdint>

#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sys/socket.h>

# include <algorithm>
# include <cstring>
# include <fuzz_helper.hpp>
# include <span>
# include <string>
# include <string_view>
# include <vector>

# include <sihd/net/IpAddr.hpp>
# include <sihd/net/linux/netlink.hpp>

# include <fuzzer/FuzzedDataProvider.h>
# include <linux/netlink.h>

using namespace sihd::net;
namespace netlink = sihd::net::linux_::netlink;

namespace
{
struct Item
{
        uint16_t type;
        std::string data;
};

constexpr size_t max_depth = 3;
constexpr size_t alignment = NLA_ALIGNTO;
constexpr size_t ipv4_width = 4;
constexpr size_t ipv6_width = 16;

bool same_bytes(std::string_view text, std::span<const uint8_t> bytes)
{
    return std::equal(text.begin(), text.end(), bytes.begin(), bytes.end(), [](char c, uint8_t byte) {
        return static_cast<uint8_t>(c) == byte;
    });
}

bool lies_inside(std::span<const uint8_t> parent, std::span<const uint8_t> inner)
{
    return inner.data() >= parent.data() && inner.data() + inner.size() <= parent.data() + parent.size();
}

template <typename T>
void check_integer(const netlink::Attribute & attribute, T (*read)(const netlink::Attribute &))
{
    const T value = read(attribute);
    if (attribute.data.size() < sizeof(T))
        return;
    T expected {};
    std::memcpy(&expected, attribute.data.data(), sizeof(T));
    test::fuzz_check(value == expected, "an integer attribute is read in host byte order");
}

void check_string(const netlink::Attribute & attribute)
{
    const std::string text = netlink::str(attribute);
    test::fuzz_check(text.size() <= attribute.data.size(), "a string attribute is not longer than its data");
    test::fuzz_check(same_bytes(text, attribute.data.first(text.size())), "a string attribute is a prefix of its data");
    test::fuzz_check(text.find('\0') == std::string::npos, "a string attribute holds no NUL");
    test::fuzz_check(text.size() == attribute.data.size() || attribute.data[text.size()] == 0,
                     "a string attribute stops at the first NUL");
}

void check_ip(const netlink::Attribute & attribute, int family, size_t width)
{
    const IpAddr addr = netlink::ip(attribute, family);
    if (attribute.data.size() != width)
    {
        test::fuzz_check(addr.empty(), "an address attribute of the wrong width is refused");
        return;
    }
    test::fuzz_check(family == AF_INET ? addr.is_ipv4() : addr.is_ipv6(), "an address attribute has its family");
    const std::vector<uint8_t> bytes = addr.bytes();
    test::fuzz_check(std::equal(bytes.begin(), bytes.end(), attribute.data.begin(), attribute.data.end()),
                     "an address attribute keeps its bytes");
}

void check_attributes(std::span<const uint8_t> parent, const std::vector<netlink::Attribute> & attributes, size_t depth)
{
    const uint8_t *previous_end = parent.data();
    for (const netlink::Attribute & attribute : attributes)
    {
        test::fuzz_check(lies_inside(parent, attribute.data), "an attribute lies inside its parent");
        test::fuzz_check(attribute.data.data() - previous_end >= NLA_HDRLEN,
                         "an attribute follows the previous one and holds its own header");
        test::fuzz_check((attribute.data.data() - parent.data()) % alignment == 0, "attribute data is aligned");
        test::fuzz_check((attribute.type & (NLA_F_NESTED | NLA_F_NET_BYTEORDER)) == 0, "the type flags are stripped");
        previous_end = attribute.data.data() + attribute.data.size();

        check_integer(attribute, netlink::u8);
        check_integer(attribute, netlink::u16);
        check_integer(attribute, netlink::u32);
        check_string(attribute);
        check_ip(attribute, AF_INET, ipv4_width);
        check_ip(attribute, AF_INET6, ipv6_width);
        if (depth < max_depth)
            check_attributes(attribute.data, netlink::nested(attribute), depth + 1);
    }
}

netlink::Message finish(netlink::Request & request)
{
    const std::vector<uint8_t> & buffer = request.finish(1);
    return {.payload = std::vector<uint8_t>(buffer.begin() + NLMSG_HDRLEN, buffer.end())};
}

netlink::Message encode(const std::vector<Item> & items)
{
    netlink::Request request(0, 0);
    for (const Item & item : items)
        request.add_attribute(item.type, item.data.data(), item.data.size());
    return finish(request);
}

std::vector<Item> consume_items(FuzzedDataProvider & provider)
{
    std::vector<Item> items(provider.ConsumeIntegralInRange<size_t>(0, 6));
    for (Item & item : items)
    {
        item.type = static_cast<uint16_t>(provider.ConsumeIntegral<uint16_t>() & NLA_TYPE_MASK);
        item.data = provider.ConsumeBytesAsString(provider.ConsumeIntegralInRange<size_t>(0, 24));
    }
    return items;
}

void check_items(const std::vector<netlink::Attribute> & attributes, const std::vector<Item> & items)
{
    test::fuzz_check(attributes.size() == items.size(), "every encoded attribute is found");
    for (size_t i = 0; i < items.size(); ++i)
    {
        test::fuzz_check(attributes[i].type == items[i].type, "an attribute keeps its type");
        test::fuzz_check(same_bytes(items[i].data, attributes[i].data), "an attribute keeps its data");
    }
}

void fuzz_raw(FuzzedDataProvider & provider)
{
    const size_t header_size = provider.ConsumeIntegralInRange<size_t>(0, 64);
    const netlink::Message message {.type = provider.ConsumeIntegral<uint16_t>(),
                                    .payload = provider.ConsumeBytes<uint8_t>(4096)};

    const size_t offset = std::min((header_size + alignment - 1) / alignment * alignment, message.payload.size());
    const std::span<const uint8_t> body = std::span<const uint8_t>(message.payload).subspan(offset);
    check_attributes(body, netlink::attributes(message, header_size), 0);
}

void fuzz_round_trip(FuzzedDataProvider & provider)
{
    const std::vector<Item> items = consume_items(provider);
    const netlink::Message message = encode(items);
    check_items(netlink::attributes(message, 0), items);

    const Item outer {.type = 1, .data = std::string(message.payload.begin(), message.payload.end())};
    const netlink::Message wrapper = encode({outer});
    const std::vector<netlink::Attribute> wrapped = netlink::attributes(wrapper, 0);
    test::fuzz_check(wrapped.size() == 1, "a wrapping attribute is found");
    check_items(netlink::nested(wrapped[0]), items);
}

void fuzz_typed_round_trip(FuzzedDataProvider & provider)
{
    const uint32_t number = provider.ConsumeIntegral<uint32_t>();
    std::string text = provider.ConsumeRandomLengthString(32);
    std::erase(text, '\0');

    netlink::Request request(0, 0);
    request.add_u32(1, number);
    request.add_string(2, text);
    const netlink::Message message = finish(request);
    const std::vector<netlink::Attribute> attributes = netlink::attributes(message, 0);
    test::fuzz_check(attributes.size() == 2, "both typed attributes are found");
    test::fuzz_check(netlink::u32(attributes[0]) == number, "an u32 attribute round trips");
    test::fuzz_check(netlink::str(attributes[1]) == text, "a string attribute round trips");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_raw(provider);
            break;
        case 1:
            fuzz_round_trip(provider);
            break;
        default:
            fuzz_typed_round_trip(provider);
            break;
    }
    return 0;
}

#else

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *, size_t)
{
    return 0;
}

#endif
