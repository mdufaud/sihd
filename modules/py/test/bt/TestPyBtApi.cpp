#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/bt/GattServer.hpp>
#include <sihd/py/bt/PyBtApi.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/util/Logger.hpp>

#include "../DirectorySwitcher.hpp"
#include "../PyInterpreter.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::py;
class TestPyBtApi: public ::testing::Test
{
    protected:
        TestPyBtApi() { LoggerManager::stream(); }

        virtual ~TestPyBtApi() { LoggerManager::clear_loggers(); }

        // libdbus reads the bus address once per process: the scripts get a system bus nobody listens on
        virtual void SetUp()
        {
            ASSERT_TRUE(sihd::sys::env::set("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent/sihd_py_bt_socket"));
        }

        virtual void TearDown() {}
};

TEST_F(TestPyBtApi, test_pybt_base)
{
    // another suite may have reached the real system bus first: only a fresh process reads the unreachable one
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();

    EXPECT_EXIT(
        {
            bool passed = true;
            try
            {
                pybind11::eval_file(dir_switcher.old_cwd() + "/test/bt/py/test_bt.py");
            }
            catch (const pybind11::error_already_set & error)
            {
                fmt::print(stderr, "{}\n", error.what());
                passed = false;
            }
            // the interpreter stays up: the child skips its teardown
            std::_Exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS),
        "");
}

// the backend calls the handlers from a thread of its own, after the python objects that set them are gone
TEST_F(TestPyBtApi, test_pybt_local_characteristic_threads)
{
    using sihd::bt::LocalCharacteristic;

    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    ASSERT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/bt/py/test_bt_callbacks.py"));

    pybind11::module_ main = pybind11::module_::import("__main__");
    std::vector<LocalCharacteristic> backend;
    for (const char *name : {"working", "raising", "mistyped"})
    {
        backend.push_back(main.attr(name).cast<LocalCharacteristic &>());
        main.attr(name) = pybind11::none();
    }

    const std::vector<uint8_t> accepted = {0x0a, 0x0b};
    const std::vector<uint8_t> refused = {0x01};
    std::vector<std::vector<uint8_t>> reads(3);
    std::vector<bool> accepted_writes(3);
    std::vector<bool> refused_writes(3);
    {
        pybind11::gil_scoped_release release;
        std::thread thread([&] {
            for (size_t i = 0; i < backend.size(); ++i)
            {
                reads[i] = backend[i].on_read();
                accepted_writes[i] = backend[i].on_write(accepted);
                refused_writes[i] = backend[i].on_write(refused);
            }
            // the last owner of the python handlers dies here, outside the GIL
            backend.clear();
        });
        thread.join();
    }

    pybind11::module_ sys = pybind11::module_::import("sys");
    sys.attr("unraisablehook") = sys.attr("__unraisablehook__");

    EXPECT_EQ(reads[0], (std::vector<uint8_t> {0x01, 0x02, 0x03}));
    EXPECT_TRUE(accepted_writes[0]);
    EXPECT_FALSE(refused_writes[0]);
    for (size_t i = 1; i < reads.size(); ++i)
    {
        EXPECT_TRUE(reads[i].empty());
        EXPECT_FALSE(accepted_writes[i]);
        EXPECT_FALSE(refused_writes[i]);
    }
    EXPECT_EQ(main.attr("backend_reads").cast<std::vector<int>>(), (std::vector<int> {1}));
    EXPECT_EQ(main.attr("backend_writes").cast<std::vector<std::string>>(),
              (std::vector<std::string> {"\x0a\x0b", "\x01"}));
    EXPECT_EQ(main.attr("unraisable").cast<std::vector<std::string>>(),
              (std::vector<std::string> {"ZeroDivisionError",
                                         "ZeroDivisionError",
                                         "ZeroDivisionError",
                                         "TypeError",
                                         "TypeError",
                                         "TypeError"}));
}
} // namespace test
