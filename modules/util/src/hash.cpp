#include <array>
#include <cstring>

#include <sihd/util/hash.hpp>

namespace sihd::util::hash
{

namespace
{

constexpr size_t block_size = 64;
constexpr size_t state_words = 5;
constexpr size_t schedule_words = 80;
constexpr size_t length_bytes = 8;

struct SHA1Context
{
        std::array<uint32_t, state_words> state;
        std::array<uint32_t, 2> count;
        std::array<uint8_t, block_size> buffer;
};

inline uint32_t rol(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32 - bits));
}

void sha1_transform(std::array<uint32_t, state_words> & state, const uint8_t *buffer)
{
    uint32_t a, b, c, d, e;
    std::array<uint32_t, schedule_words> w;

    for (int i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)buffer[i * 4] << 24) | ((uint32_t)buffer[i * 4 + 1] << 16)
               | ((uint32_t)buffer[i * 4 + 2] << 8) | ((uint32_t)buffer[i * 4 + 3]);
    }

    for (int i = 16; i < 80; i++)
    {
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    for (int i = 0; i < 80; i++)
    {
        uint32_t f, k;

        if (i < 20)
        {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        }
        else if (i < 40)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        }
        else if (i < 60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void sha1_init(SHA1Context *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

void sha1_update(SHA1Context *ctx, const uint8_t *data, size_t len)
{
    size_t i, j;

    j = (ctx->count[0] >> 3) & 63;

    const uint64_t bits = (uint64_t)len << 3;
    const uint32_t previous = ctx->count[0];
    ctx->count[0] = previous + (uint32_t)bits;
    ctx->count[1] += (uint32_t)(bits >> 32) + (ctx->count[0] < previous ? 1u : 0u);

    if ((j + len) > 63)
    {
        i = 64 - j;
        memcpy(&ctx->buffer[j], data, i);
        sha1_transform(ctx->state, ctx->buffer.data());

        for (; i + 63 < len; i += 64)
        {
            sha1_transform(ctx->state, &data[i]);
        }

        j = 0;
    }
    else
    {
        i = 0;
    }

    if (len > i)
        memcpy(&ctx->buffer[j], &data[i], len - i);
}

void sha1_final(uint8_t digest[sha1_size], SHA1Context *ctx)
{
    std::array<uint8_t, length_bytes> finalcount;

    for (size_t i = 0; i < finalcount.size(); i++)
    {
        finalcount[i] = (uint8_t)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    }

    sha1_update(ctx, (const uint8_t *)"\x80", 1);

    while ((ctx->count[0] & 504) != 448)
    {
        sha1_update(ctx, (const uint8_t *)"\0", 1);
    }

    sha1_update(ctx, finalcount.data(), finalcount.size());

    for (size_t i = 0; i < sha1_size; i++)
    {
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}

} // namespace

void sha1(const uint8_t *data, size_t len, uint8_t output[sha1_size])
{
    SHA1Context ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(output, &ctx);
}

} // namespace sihd::util::hash
