#include <array>
#include <cstdint>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <sihd/util/Regex.hpp>

namespace sihd::util
{

namespace
{

struct CodeDeleter
{
        void operator()(pcre2_code *code) const { pcre2_code_free(code); }
};

struct MatchDataDeleter
{
        void operator()(pcre2_match_data *data) const { pcre2_match_data_free(data); }
};

using MatchData = std::unique_ptr<pcre2_match_data, MatchDataDeleter>;

// pcre2 rejects a null pointer even for an empty string_view
PCRE2_SPTR to_sptr(std::string_view view)
{
    return reinterpret_cast<PCRE2_SPTR>(view.data() != nullptr ? view.data() : "");
}

std::string error_message(int code)
{
    std::array<PCRE2_UCHAR, 256> buffer {};
    const int len = pcre2_get_error_message(code, buffer.data(), buffer.size());
    if (len < 0)
        return fmt::format("pcre2 error {}", code);
    return std::string(buffer.begin(), buffer.begin() + len);
}

Error match_error(int rc)
{
    switch (rc)
    {
        case PCRE2_ERROR_MATCHLIMIT:
        case PCRE2_ERROR_DEPTHLIMIT:
        case PCRE2_ERROR_HEAPLIMIT:
        case PCRE2_ERROR_JIT_STACKLIMIT:
            return Error(ErrorCode::overflow, "regex limit reached: {}", error_message(rc));
        case PCRE2_ERROR_NOMEMORY:
            return Error(ErrorCode::out_of_memory, "regex out of memory: {}", error_message(rc));
        default:
            return Error(ErrorCode::invalid_argument, "regex failed: {}", error_message(rc));
    }
}

// the jit stack is small and fixed: the interpreter only bounded by the heap limit takes over
int run_match(const pcre2_code *code, std::string_view subject, size_t offset, uint32_t options, pcre2_match_data *data)
{
    int rc = pcre2_match(code, to_sptr(subject), subject.size(), offset, options, data, nullptr);
    if (rc == PCRE2_ERROR_JIT_STACKLIMIT)
        rc = pcre2_match(code, to_sptr(subject), subject.size(), offset, options | PCRE2_NO_JIT, data, nullptr);
    return rc;
}

int run_substitute(const pcre2_code *code,
                   std::string_view subject,
                   std::string_view replacement,
                   uint32_t options,
                   std::vector<PCRE2_UCHAR> & buffer,
                   PCRE2_SIZE & out_len)
{
    int rc = pcre2_substitute(code,
                              to_sptr(subject),
                              subject.size(),
                              0,
                              options,
                              nullptr,
                              nullptr,
                              to_sptr(replacement),
                              replacement.size(),
                              buffer.data(),
                              &out_len);
    if (rc == PCRE2_ERROR_JIT_STACKLIMIT)
    {
        out_len = buffer.size();
        rc = pcre2_substitute(code,
                              to_sptr(subject),
                              subject.size(),
                              0,
                              options | PCRE2_NO_JIT,
                              nullptr,
                              nullptr,
                              to_sptr(replacement),
                              replacement.size(),
                              buffer.data(),
                              &out_len);
    }
    return rc;
}

} // namespace

struct Regex::Impl
{
        std::unique_ptr<pcre2_code, CodeDeleter> code;
};

Regex::Regex(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Regex::Regex(Regex &&) noexcept = default;

Regex & Regex::operator=(Regex &&) noexcept = default;

Regex::~Regex() = default;

Result<Regex> Regex::compile(std::string_view pattern, Mode mode)
{
    const uint32_t options = mode == Mode::full ? (PCRE2_ANCHORED | PCRE2_ENDANCHORED) : 0;
    int error_code = 0;
    PCRE2_SIZE error_offset = 0;
    pcre2_code *code = pcre2_compile(to_sptr(pattern), pattern.size(), options, &error_code, &error_offset, nullptr);
    if (code == nullptr)
    {
        return std::unexpected(Error(ErrorCode::invalid_argument,
                                     "invalid regex '{}': {} at offset {}",
                                     pattern,
                                     error_message(error_code),
                                     error_offset));
    }
    // no jit support on the platform or out of memory: pcre2_match interprets the pattern instead
    pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
    auto impl = std::make_unique<Impl>();
    impl->code.reset(code);
    return Regex(std::move(impl));
}

Result<bool> Regex::matches(std::string_view subject) const
{
    const MatchData data(pcre2_match_data_create_from_pattern(_impl->code.get(), nullptr));
    if (data == nullptr)
        return std::unexpected(Error(ErrorCode::out_of_memory, "regex out of memory"));
    const int rc = run_match(_impl->code.get(), subject, 0, 0, data.get());
    if (rc >= 0)
        return true;
    if (rc == PCRE2_ERROR_NOMATCH)
        return false;
    return std::unexpected(match_error(rc));
}

Result<std::vector<std::string>> Regex::find_all(std::string_view subject) const
{
    const MatchData data(pcre2_match_data_create_from_pattern(_impl->code.get(), nullptr));
    if (data == nullptr)
        return std::unexpected(Error(ErrorCode::out_of_memory, "regex out of memory"));
    const PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(data.get());

    std::vector<std::string> found;
    size_t offset = 0;
    uint32_t options = 0;
    while (offset <= subject.size())
    {
        const int rc = run_match(_impl->code.get(), subject, offset, options, data.get());
        if (rc == PCRE2_ERROR_NOMATCH)
        {
            if (options == 0)
                break;
            options = 0;
            ++offset;
            continue;
        }
        if (rc < 0)
            return std::unexpected(match_error(rc));
        found.emplace_back(subject.substr(ovector[0], ovector[1] - ovector[0]));
        offset = ovector[1];
        options = ovector[0] == ovector[1] ? (PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED) : 0;
    }
    return found;
}

Result<std::string> Regex::replace_all(std::string_view subject, std::string_view replacement) const
{
    constexpr uint32_t options = PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_UNSET_EMPTY
                                 | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;
    std::vector<PCRE2_UCHAR> buffer(subject.size() + 16);
    PCRE2_SIZE out_len = buffer.size();
    int rc = run_substitute(_impl->code.get(), subject, replacement, options, buffer, out_len);
    if (rc == PCRE2_ERROR_NOMEMORY)
    {
        buffer.resize(out_len);
        rc = run_substitute(_impl->code.get(), subject, replacement, options, buffer, out_len);
    }
    if (rc < 0)
        return std::unexpected(match_error(rc));
    return std::string(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(out_len));
}

} // namespace sihd::util
