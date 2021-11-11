#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/OS.hpp>

NEW_LOGGER("test::module");

using namespace sihd::util;

int main()
{
    LoggerManager::basic();

    Node node("root");
    LOG(info, "Hello: " << node.get_name());
    std::cout << node.get_name() << std::endl;
    
    LOG(info, OS::get_executable_path());

    Waitable wait;
    wait.wait_for(time::sec(5));

    return 0;
}