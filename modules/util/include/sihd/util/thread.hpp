#ifndef __SIHD_UTIL_THREAD_HPP__
#define __SIHD_UTIL_THREAD_HPP__

#include <pthread.h>

#include <string>
#include <thread>

#include <sihd/util/Result.hpp>

namespace sihd::util::thread
{

pthread_t id();
pthread_t main();
std::string id_str(pthread_t id = thread::id());
sihd::util::Result<void> set_name(const std::string & name);
// never throws: the tracked name is the fallback when the platform value is unreadable
const std::string & name();
bool equals(const pthread_t & id1, const pthread_t & id2);

} // namespace sihd::util::thread

#endif
