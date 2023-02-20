#ifndef __SIHD_UTIL_PATH_HPP__
#define __SIHD_UTIL_PATH_HPP__

#include <string>

namespace sihd::util::path
{

void clear_all();
void clear(const std::string & url);
void set(const std::string & url, const std::string & path);
std::string get(const std::string & path);
std::string get(const std::string & url, const std::string & path);
std::string get_from(const std::string & from, const std::string & path);
std::string find(const std::string & path);

std::string url_delimiter();

} // namespace sihd::util::path

#endif