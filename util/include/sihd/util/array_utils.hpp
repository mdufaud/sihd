#ifndef __SIHD_UTIL_ARRAYUTILS_HPP__
#define __SIHD_UTIL_ARRAYUTILS_HPP__

#include <stdexcept> // out of range
#include <vector>

#include <sihd/util/IArray.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util::array_utils
{

/**
 * @brief assign pointers of a distributing array to a list of arrays
 *  distributing_array: size 11 bytes [0x00 ... 0x0b]
 *  assigned_arrays: [ [float_array, 2], [short_array, 1] ]
 *  starting_offset = 1
 *
 * outputs:
 *  float_array (4 bytes): size 2 [0x01 ... 0x09]
 *  short_array (2 bytes): size 1 [0x09 ... 0x0b]
 *
 * throws std::invalid_argument if byte size is not aligned with the data size
 *
 * @param distributing_array the array containing the buffer to distribute to other arrays
 * @param assigned_arrays a vector of array-size pairs that will hold sequentially distributed pointer
 * @param starting_offset the starting byte offset of distributing_array starting byte distribution
 * @return true distribution is done
 * @return false either starting_offset is too high or distributing_array has not enough
 *  capacity to fill assigned_arrays
 */
bool distribute_array(IArray & distributing_array,
                      const std::vector<std::pair<IArray *, size_t>> assigned_arrays,
                      size_t starting_offset = 0);

IArray *create_from_type(Type dt, size_t size = 0);

template <typename T>
bool read_into(const IArray & arr, size_t idx, T & val)
{
    static_assert(std::is_trivially_copyable_v<T>);
    return arr.copy_to_bytes(&val, sizeof(T), arr.byte_index(idx));
}

template <typename T>
bool read_into(const IArray *arr, size_t idx, T & val)
{
    return array_utils::read_into<T>(*arr, idx, val);
}

// throws std::invalid_argument if data cannot be read
template <typename T>
T read(const IArray & arr, size_t idx)
{
    T ret;

    if (array_utils::read_into<T>(arr, idx, ret) == false)
    {
        throw std::invalid_argument(
            str::format("array_utils::read cannot copy data from idx %lu into type '%s' (array type: '%s')",
                        idx,
                        type::str<T>(),
                        arr.data_type_str()));
    }
    return ret;
}

template <typename T>
T read(const IArray *arr, size_t idx)
{
    return array_utils::read<T>(*arr, idx);
}

template <typename T>
bool write(IArray & arr, size_t idx, T value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    return arr.copy_from_bytes(&value, sizeof(T), arr.byte_index(idx));
}

template <typename T>
bool write(IArray *arr, size_t idx, T value)
{
    return array_utils::write<T>(*arr, idx, value);
}

} // namespace sihd::util::array_utils

#endif