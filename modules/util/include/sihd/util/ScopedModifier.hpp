#ifndef __SIHD_UTIL_SCOPEDMODIFIER_HPP__
#define __SIHD_UTIL_SCOPEDMODIFIER_HPP__

namespace sihd::util
{

template <typename T, typename V>
class ScopedModifier
{
    public:
        ScopedModifier(T & val, V new_val): _ref(val), _old_val(val) { val = new_val; }
        ~ScopedModifier() { _ref = _old_val; }

    protected:

    private:
        T & _ref;
        V _old_val;
};

} // namespace sihd::util

#endif