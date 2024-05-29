#pragma once

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
class IObservable;

template <typename T>
class IProvider;

class ISteppable;

template <typename T>
class Array;

typedef Array<bool> ArrBool;
typedef Array<char> ArrChar;
typedef Array<int8_t> ArrByte;
typedef Array<uint8_t> ArrUByte;
typedef Array<int16_t> ArrShort;
typedef Array<uint16_t> ArrUShort;
typedef Array<int32_t> ArrInt;
typedef Array<uint32_t> ArrUInt;
typedef Array<int64_t> ArrLong;
typedef Array<uint64_t> ArrULong;
typedef Array<float> ArrFloat;
typedef Array<double> ArrDouble;

template <typename T>
class ArrayView;

typedef ArrayView<bool> ArrBoolView;
typedef ArrayView<char> ArrCharView;
typedef ArrayView<int8_t> ArrByteView;
typedef ArrayView<uint8_t> ArrUByteView;
typedef ArrayView<int16_t> ArrShortView;
typedef ArrayView<uint16_t> ArrUShortView;
typedef ArrayView<int32_t> ArrIntView;
typedef ArrayView<uint32_t> ArrUIntView;
typedef ArrayView<int64_t> ArrLongView;
typedef ArrayView<uint64_t> ArrULongView;
typedef ArrayView<float> ArrFloatView;
typedef ArrayView<double> ArrDoubleView;

class Named;
class Node;
class Timestamp;

class MessageField;
class Message;
class DynMessage;

class Waitable;
class Synchronizer;

class SystemClock;
class SteadyClock;

class Task;
class Defer;

} // namespace sihd::util