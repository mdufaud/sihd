#ifndef __SIHD_PY_PYUTILAPI_HPP__
#define __SIHD_PY_PYUTILAPI_HPP__

#include <sihd/py/PyApi.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/util/Task.hpp>

#include <pybind11/chrono.h>
#include <pybind11/stl.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, sihd::util::SmartNodePtr<T>);

namespace sihd::py
{

class PyUtilApi
{
    public:
        static void add_util_api(PyApi::PyModule & pymodule);

        class __attribute__((visibility("hidden"))) PyTask: public sihd::util::Task
        {
            public:
                PyTask(const pybind11::function & task, const sihd::util::TaskOptions & options);
                virtual ~PyTask();

                virtual bool run();

            private:
                pybind11::function _fun;
        };

    protected:
        static bool _configurable_set_conf(sihd::util::Configurable *self, const pybind11::kwargs & kwargs);
        static bool _configurable_set_single_conf(sihd::util::Configurable *self,
                                                  const std::string & key,
                                                  const pybind11::handle & handle);

        // Array utils

        template <typename T>
        static bool _array_py_push_back_list(sihd::util::Array<T> & self, pybind11::list list)
        {
            bool ret = true;
            for (const auto & item : list)
            {
                if (self.push_back(item.cast<T>()) == false)
                {
                    ret = false;
                    break;
                }
            }
            return ret;
        }

        template <typename T>
        static bool _array_py_push_back_tuple(sihd::util::Array<T> & self, pybind11::tuple tuple)
        {
            bool ret = true;
            for (const auto & item : tuple)
            {
                if (self.push_back(item.cast<T>()) == false)
                {
                    ret = false;
                    break;
                }
            }
            return ret;
        }

        template <typename T>
        static bool _array_py_push_front_list(sihd::util::Array<T> & self, pybind11::list list)
        {
            bool ret = true;
            for (const auto & item : list)
            {
                if (self.push_front(item.cast<T>()) == false)
                {
                    ret = false;
                    break;
                }
            }
            return ret;
        }

        template <typename T>
        static bool _array_py_push_front_tuple(sihd::util::Array<T> & self, pybind11::tuple tuple)
        {
            bool ret = true;
            for (const auto & item : tuple)
            {
                if (self.push_front(item.cast<T>()) == false)
                {
                    ret = false;
                    break;
                }
            }
            return ret;
        }

        template <typename T>
        static T _array_py_getitem(const sihd::util::Array<T> & self, size_t idx)
        {
            if (idx >= self.size())
                throw pybind11::index_error();
            return self.at(idx);
        }

        template <typename T>
        static pybind11::iterator _array_py_iter(const sihd::util::Array<T> & self)
        {
            return pybind11::make_iterator(self.cbegin(), self.cend());
        }

        template <typename T>
        static bool _array_py_contains(const sihd::util::Array<T> & self, T value)
        {
            return std::find(self.cbegin(), self.cend(), value) != self.cend();
        }

        template <typename T>
        static sihd::util::Array<T> _array_py_reversed(const sihd::util::Array<T> & self)
        {
            sihd::util::Array<T> ret;
            ret.resize(self.size());
            std::copy(self.crbegin(), self.crend(), ret.begin());
            return ret;
        }

    private:
        ~PyUtilApi() {};
};

} // namespace sihd::py

#endif
