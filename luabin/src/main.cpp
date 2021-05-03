#include <iostream>
#include <sihd/core/Named.hpp>

int     main(void)
{
    sihd::core::Named obj("test");

    std::cout << obj.get_name() << std::endl;
    return 0;
}