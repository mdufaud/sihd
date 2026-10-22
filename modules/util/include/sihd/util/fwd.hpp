#ifndef __SIHD_UTIL_FWD_HPP__
#define __SIHD_UTIL_FWD_HPP__

#include <cstdint>

#include <sihd/util/traits.hpp>

namespace sihd::util
{

class IClock;
class IArray;
class IArrayView;
class IMessageField;
class IRunnable;
class IReader;
class IWriter;

template <typename... T>
class IHandler;

template <typename T>
class IProvider;

class ISteppable;

template <traits::TriviallyCopyable T>
class Array;

using ArrBool = Array<bool>;
using ArrChar = Array<char>;
using ArrByte = Array<int8_t>;
using ArrUByte = Array<uint8_t>;
using ArrShort = Array<int16_t>;
using ArrUShort = Array<uint16_t>;
using ArrInt = Array<int32_t>;
using ArrUInt = Array<uint32_t>;
using ArrLong = Array<int64_t>;
using ArrULong = Array<uint64_t>;
using ArrFloat = Array<float>;
using ArrDouble = Array<double>;

template <traits::TriviallyCopyable T>
class ArrayView;

using ArrBoolView = ArrayView<bool>;
using ArrCharView = ArrayView<char>;
using ArrByteView = ArrayView<int8_t>;
using ArrUByteView = ArrayView<uint8_t>;
using ArrShortView = ArrayView<int16_t>;
using ArrUShortView = ArrayView<uint16_t>;
using ArrIntView = ArrayView<int32_t>;
using ArrUIntView = ArrayView<uint32_t>;
using ArrLongView = ArrayView<int64_t>;
using ArrULongView = ArrayView<uint64_t>;
using ArrFloatView = ArrayView<float>;
using ArrDoubleView = ArrayView<double>;

class Named;
class Node;
class Timestamp;
class Duration;

class MessageField;
class Message;
class DynMessage;

class SystemClock;
class SteadyClock;

class Task;
class Defer;

} // namespace sihd::util

#endif