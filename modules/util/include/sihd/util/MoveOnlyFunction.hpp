#ifndef __SIHD_UTIL_MOVE_ONLY_FUNCTION_HPP__
#define __SIHD_UTIL_MOVE_ONLY_FUNCTION_HPP__

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#if defined(__cpp_lib_move_only_function)

namespace sihd::util
{

template <typename T>
using MoveOnlyFunction = std::move_only_function<T>;

} // namespace sihd::util

#else

// libc++ never shipped std::move_only_function (macro listed unimplemented, implementation
// PR llvm#94670 still open): stand-in until it lands. Empty invoke throws here — the standard
// leaves it unspecified and libstdc++ aborts
namespace sihd::util
{

template <typename T>
class MoveOnlyFunction;

template <typename R, typename... A>
class MoveOnlyFunction<R(A...)>
{
    public:
        MoveOnlyFunction() = default;
        MoveOnlyFunction(std::nullptr_t) {}
        MoveOnlyFunction(MoveOnlyFunction &&) = default;
        MoveOnlyFunction & operator=(MoveOnlyFunction &&) = default;
        MoveOnlyFunction(const MoveOnlyFunction &) = delete;
        MoveOnlyFunction & operator=(const MoveOnlyFunction &) = delete;

        template <typename F>
        MoveOnlyFunction(F && functor): _impl(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(functor)))
        {
        }

        MoveOnlyFunction & operator=(std::nullptr_t)
        {
            _impl = nullptr;
            return *this;
        }

        explicit operator bool() const { return _impl != nullptr; }

        friend bool operator==(const MoveOnlyFunction & fn, std::nullptr_t) { return fn._impl == nullptr; }

        R operator()(A... args)
        {
            if (_impl == nullptr)
                throw std::bad_function_call();
            return _impl->call(args...);
        }

    private:
        struct Base
        {
                virtual ~Base() = default;
                virtual R call(A...) = 0;
        };

        template <typename F>
        struct Model: Base
        {
                F functor;

                explicit Model(F && functor): functor(std::move(functor)) {}

                R call(A... args) override { return this->functor(args...); }
        };

        std::unique_ptr<Base> _impl;
};

} // namespace sihd::util

#endif

#endif
