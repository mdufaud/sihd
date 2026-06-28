#ifndef __SIHD_UTIL_THREAD_HPP__
#define __SIHD_UTIL_THREAD_HPP__

#include <thread>

namespace sihd::util::thread
{

pthread_t id();
pthread_t main();
std::string id_str(pthread_t id = thread::id());
void set_name(const std::string & name);
const std::string & name();
bool equals(const pthread_t & id1, const pthread_t & id2);

} // namespace sihd::util::thread

#endif
