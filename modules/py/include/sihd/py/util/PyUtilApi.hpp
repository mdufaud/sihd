#ifndef __SIHD_PY_PYUTILAPI_HPP__
#define __SIHD_PY_PYUTILAPI_HPP__

#include <pybind11/chrono.h>
#include <pybind11/stl.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <sihd/py/PyApi.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/util/Task.hpp>

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

        // a python function a backend thread calls: copies of it take no GIL, the last one releases it under the GIL
        class __attribute__((visibility("hidden"))) PyCallback
        {
            public:
                explicit PyCallback(pybind11::function function);

                // an error the function raises is reported as unraisable: it cannot cross the backend thread
                template <typename Call>
                void run(Call && call) const
                {
                    pybind11::gil_scoped_acquire acquire;
                    try
                    {
                        call(*_function);
                    }
                    catch (pybind11::error_already_set & error)
                    {
                        error.discard_as_unraisable("sihd callback");
                    }
                    catch (const std::exception & error)
                    {
                        // a result of the wrong type: pybind11 throws its own exceptions, not a python error
                        pybind11::set_error(PyExc_TypeError, error.what());
                        pybind11::error_already_set().discard_as_unraisable("sihd callback");
                    }
                }

            private:
                std::shared_ptr<pybind11::function> _function;
        };

        // the python functions observing an Observable<T>, until remove_all
        template <typename T>
        class __attribute__((visibility("hidden"))) PyObservers
        {
            public:
                static void add(T & owner, pybind11::function function)
                {
                    auto handler = std::make_unique<Handler>(std::move(function));
                    Handler *added = handler.get();
                    {
                        std::lock_guard lock(mutex());
                        handlers().emplace(&owner, std::move(handler));
                    }
                    pybind11::gil_scoped_release release;
                    owner.add_observer(added);
                }

                static void remove_all(T & owner)
                {
                    std::vector<std::unique_ptr<Handler>> removed;
                    {
                        std::lock_guard lock(mutex());
                        const auto [first, last] = handlers().equal_range(&owner);
                        for (auto it = first; it != last; ++it)
                            removed.push_back(std::move(it->second));
                        handlers().erase(first, last);
                    }
                    pybind11::gil_scoped_release release;
                    for (const auto & handler : removed)
                        owner.remove_observer(handler.get());
                }

            private:
                class Handler: public sihd::util::IHandler<T *>
                {
                    public:
                        explicit Handler(pybind11::function function): _callback(std::move(function)) {}

                        void handle(T *owner) override
                        {
                            _callback.run([owner](pybind11::function & function) {
                                function(pybind11::cast(owner, pybind11::return_value_policy::reference));
                            });
                        }

                    private:
                        PyCallback _callback;
                };

                static std::mutex & mutex()
                {
                    static std::mutex instance;
                    return instance;
                }

                // never destroyed: the end of the interpreter forbids releasing a python object
                static std::multimap<const T *, std::unique_ptr<Handler>> & handlers()
                {
                    static auto & instance = *new std::multimap<const T *, std::unique_ptr<Handler>>;
                    return instance;
                }
        };

    protected:
        static bool _configurable_set_conf(sihd::util::Configurable *self, const pybind11::kwargs & kwargs);
        static bool _configurable_set_single_conf(sihd::util::Configurable *self,
                                                  const std::string & key,
                                                  const pybind11::handle & handle);

    private:
        ~PyUtilApi() = default;
        ;
};

} // namespace sihd::py

#endif
