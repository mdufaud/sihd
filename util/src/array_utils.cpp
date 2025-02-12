#include <sihd/util/Array.hpp>
#include <sihd/util/array_utils.hpp>

namespace sihd::util::array_utils
{

bool distribute_array(IArray & distributing_array,
                      const std::vector<std::pair<IArray *, size_t>> & assigned_arrays,
                      size_t starting_offset)
{
    if (starting_offset > distributing_array.byte_size())
    {
        throw std::invalid_argument(
            str::format("array_utils: starting offset is beyond distributing array size (%lu > %lu)",
                        starting_offset,
                        distributing_array.byte_size()));
    }
    size_t total = 0;
    for (const auto & pair : assigned_arrays)
        total += pair.second * pair.first->data_size();
    if ((total + starting_offset) > distributing_array.byte_size())
    {
        throw std::invalid_argument(str::format("array_utils: total distribution exceed array size (%lu > %lu)",
                                                total + starting_offset,
                                                distributing_array.byte_size()));
    }
    size_t distributed_byte_size;
    size_t offset_idx = starting_offset;
    for (const auto & pair : assigned_arrays)
    {
        distributed_byte_size = pair.second * pair.first->data_size();
        if (pair.first->assign_bytes(distributing_array.buf() + offset_idx, distributed_byte_size) == false)
            return false;
        offset_idx += distributed_byte_size;
    }
    return true;
}

IArray *create_from_type(Type dt, size_t size)
{
    switch (dt)
    {
        case TYPE_BOOL:
            return new Array<bool>(size);
        case TYPE_CHAR:
            return new Array<char>(size);
        case TYPE_BYTE:
            return new Array<int8_t>(size);
        case TYPE_UBYTE:
            return new Array<uint8_t>(size);
        case TYPE_SHORT:
            return new Array<int16_t>(size);
        case TYPE_USHORT:
            return new Array<uint16_t>(size);
        case TYPE_INT:
            return new Array<int32_t>(size);
        case TYPE_UINT:
            return new Array<uint32_t>(size);
        case TYPE_LONG:
            return new Array<int64_t>(size);
        case TYPE_ULONG:
            return new Array<uint64_t>(size);
        case TYPE_FLOAT:
            return new Array<float>(size);
        case TYPE_DOUBLE:
            return new Array<double>(size);
        default:
            break;
    }
    return nullptr;
}

} // namespace sihd::util::array_utils