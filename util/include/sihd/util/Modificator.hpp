#ifndef __SIHD_UTIL_MODIFICATOR_HPP__
# define __SIHD_UTIL_MODIFICATOR_HPP__

# include <functional>

namespace sihd::util
{

template <typename T, typename V>
class Modificator
{
    public:
        Modificator(T & val, V new_val): _ref(val)
        {
            _old_val = val;
            val = new_val;
        }

        ~Modificator()
        {
            _ref.get() = _old_val;
        }

    protected:

    private:
        V _old_val;
        std::reference_wrapper<T> _ref;

};

}

#endif