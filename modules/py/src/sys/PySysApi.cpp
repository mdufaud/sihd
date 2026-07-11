#include <pybind11/stl.h>

#include <sihd/py/sys/PySysApi.hpp>
#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/sys/Uuid.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/screenshot.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::py
{

using namespace sihd::sys;
SIHD_LOGGER;

namespace
{
// from path/bin/script.py -> path/bin -> path
std::string g_exe_dir = fs::parent(fs::parent(fs::executable_path()));
} // namespace

void PySysApi::add_sys_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();

    m_sihd.attr("dir") = g_exe_dir;

    pybind11::module m_sys = m_sihd.def_submodule("sys", "sihd::sys");

    m_sys.def_submodule("fs", "sihd::sys::fs")
        .def("exists", &fs::exists)
        .def("is_file", &fs::is_file)
        .def("is_dir", &fs::is_dir)
        .def("file_size", &fs::file_size)
        .def("remove_directory", &fs::remove_directory)
        .def("remove_directories", &fs::remove_directories)
        .def("make_directory", &fs::make_directory, pybind11::arg("path"), pybind11::arg("mode") = 0750)
        .def("make_directories", &fs::make_directories, pybind11::arg("path"), pybind11::arg("mode") = 0750)
        .def("children", &fs::children)
        .def("recursive_children", &fs::recursive_children)
        .def("is_absolute", &fs::is_absolute)
        .def("normalize", &fs::normalize)
        .def("trim_path", static_cast<std::string (*)(std::string_view, std::string_view)>(&fs::trim_path))
        .def("parent", &fs::parent)
        .def("filename", &fs::filename)
        .def("extension", &fs::extension)
        .def("combine", static_cast<std::string (*)(std::string_view, std::string_view)>(&fs::combine))
        .def("remove_file", &fs::remove_file)
        .def("are_equals", &fs::are_equals)
        .def("home_path", &fs::home_path)
        .def("executable_path", &fs::executable_path)
        .def("cwd", &fs::cwd);

    m_sys.def_submodule("os", "sihd::sys::os")
        .def("peak_rss", &os::peak_rss)
        .def("current_rss", &os::current_rss)
        .def("is_root", &os::is_root)
        .def("pid", &os::pid)
        .def("max_fds", &os::max_fds)
        .def(
            "boot_time",
            +[]() -> int64_t { return os::boot_time(); })
        .def("exists_in_path", &os::exists_in_path)
        .def("last_error_str", &os::last_error_str)
        .def("is_run_by_valgrind", &os::is_run_by_valgrind)
        .def("is_run_by_debugger", &os::is_run_by_debugger);

    auto m_signal = m_sys.def_submodule("signal", "sihd::sys::signal");
    m_signal.def("handle", &signal::handle)
        .def("unhandle", &signal::unhandle)
        .def("ignore", &signal::ignore)
        .def("is_category_stop", &signal::is_category_stop)
        .def("is_category_termination", &signal::is_category_termination)
        .def("is_category_dump", &signal::is_category_dump)
        .def("last_received", &signal::last_received)
        .def("stop_received", &signal::stop_received)
        .def("termination_received", &signal::termination_received)
        .def("dump_received", &signal::dump_received)
        .def("should_stop", &signal::should_stop)
        .def("reset_received", &signal::reset_received)
        .def("reset_all_received", &signal::reset_all_received)
        .def("name", &signal::name);

    pybind11::class_<Uuid>(m_sys, "Uuid")
        .def(pybind11::init<>())
        .def(pybind11::init<const std::string &>())
        .def(pybind11::init<const Uuid &, const std::string &>())
        .def("is_null", &Uuid::is_null)
        .def("clear", &Uuid::clear)
        .def("__str__", &Uuid::str)
        .def("__repr__", &Uuid::str)
        .def("__bool__", &Uuid::operator bool)
        .def("__eq__", &Uuid::operator==)
        .def_static("DNS", &Uuid::DNS)
        .def_static("URL", &Uuid::URL)
        .def_static("OID", &Uuid::OID)
        .def_static("X500", &Uuid::X500);

    pybind11::class_<ProcessInfo>(m_sys, "ProcessInfo")
        .def(pybind11::init<int>())
        .def(pybind11::init<std::string_view>())
        .def("is_alive", &ProcessInfo::is_alive)
        .def("pid", &ProcessInfo::pid)
        .def("name", &ProcessInfo::name)
        .def("cwd", &ProcessInfo::cwd)
        .def("exe_path", &ProcessInfo::exe_path)
        .def("cmd_line", &ProcessInfo::cmd_line)
        .def("env", &ProcessInfo::env)
        .def(
            "creation_time",
            +[](const ProcessInfo & self) -> int64_t { return self.creation_time(); })
        .def_static("get_all_process_from_name", &ProcessInfo::get_all_process_from_name);
    // NOTE: no Process binding for Python on purpose - the stdlib `subprocess`
    // module already launches and manages processes. Bindings only cover what the
    // host language lacks (see the Lua Process binding for the no-stdlib case).

    pybind11::class_<Pixel>(m_sys, "Pixel")
        .def(pybind11::init<>())
        .def(pybind11::init<uint32_t>())
        .def_static("rgb", &Pixel::rgb)
        .def_property_readonly("value", [](const Pixel & self) { return self.value; })
        .def_property_readonly("red", [](const Pixel & self) { return self.red; })
        .def_property_readonly("green", [](const Pixel & self) { return self.green; })
        .def_property_readonly("blue", [](const Pixel & self) { return self.blue; })
        .def_property_readonly("alpha", [](const Pixel & self) { return self.alpha; });

    pybind11::class_<Bitmap>(m_sys, "Bitmap")
        .def(pybind11::init<>())
        .def(pybind11::init<size_t, size_t, uint8_t>(),
             pybind11::arg("width"),
             pybind11::arg("height"),
             pybind11::arg("bit_per_pixel") = 32)
        .def("create",
             &Bitmap::create,
             pybind11::arg("width"),
             pybind11::arg("height"),
             pybind11::arg("bit_per_pixel") = 32)
        .def("fill", &Bitmap::fill)
        .def("clear", &Bitmap::clear)
        .def("set", static_cast<void (Bitmap::*)(size_t, size_t, Pixel)>(&Bitmap::set))
        .def("get", &Bitmap::get)
        .def("is_accessible", &Bitmap::is_accessible)
        .def("save_bmp", &Bitmap::save_bmp)
        .def("read_bmp", &Bitmap::read_bmp)
        .def("to_bmp_data",
             [](const Bitmap & self) {
                 auto data = self.to_bmp_data();
                 return pybind11::bytes(reinterpret_cast<const char *>(data.data()), data.size());
             })
        .def("read_bmp_data",
             [](Bitmap & self, pybind11::bytes data) {
                 std::string bytes = data;
                 return self.read_bmp_data(Bitmap::Pixels(bytes.begin(), bytes.end()));
             })
        .def("empty", &Bitmap::empty)
        .def("width", &Bitmap::width)
        .def("height", &Bitmap::height)
        .def("byte_per_pixel", &Bitmap::byte_per_pixel);

    pybind11::module m_screenshot = m_sys.def_submodule("screenshot", "sihd::sys::screenshot");
    m_screenshot.attr("supported") = screenshot::supported;
    m_screenshot.def("take_screen", &screenshot::take_screen);
    m_screenshot.def("take_under_cursor", &screenshot::take_under_cursor);
    m_screenshot.def("take_focused", &screenshot::take_focused);
    m_screenshot.def("take_window_name", &screenshot::take_window_name);
}

static void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_sys", &PySysApi::add_sys_api);
}

} // namespace sihd::py
