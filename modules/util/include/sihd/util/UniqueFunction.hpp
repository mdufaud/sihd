#ifndef __SIHD_UTIL_UNIQUE_FUNCTION_HPP__
#define __SIHD_UTIL_UNIQUE_FUNCTION_HPP__

#include <memory>
#include <type_traits>
#include <utility>

namespace sihd::util
{

// Move-only callable: std::function requires copyable callables, libc++ 21 removed std::move_only_function
template <typename T>
class UniqueFunction;

template <typename R, typename... A>
class UniqueFunction<R(A...)>
{
    public:
        UniqueFunction() = default;
        UniqueFunction(std::nullptr_t) {}
        UniqueFunction(UniqueFunction &&) = default;
        UniqueFunction & operator=(UniqueFunction &&) = default;
        UniqueFunction(const UniqueFunction &) = delete;
        UniqueFunction & operator=(const UniqueFunction &) = delete;

        template <typename F>
        UniqueFunction(F && functor): _impl(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(functor)))
        {
        }

        UniqueFunction & operator=(std::nullptr_t)
        {
            _impl = nullptr;
            return *this;
        }

        explicit operator bool() const { return _impl != nullptr; }

        friend bool operator==(const UniqueFunction & fn, std::nullptr_t) { return fn._impl == nullptr; }

        R operator()(A... args) { return _impl->call(args...); }

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
