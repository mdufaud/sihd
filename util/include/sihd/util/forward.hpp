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
template <typename ...T>
class IHandler;
template <typename T>
class IObservable;
template <typename T>
class IProvider;
class ISteppable;
class IStoppableRunnable;

template <typename T>
class Array;

typedef Array<bool>       ArrBool;
typedef Array<char>       ArrChar;
typedef Array<int8_t>     ArrByte;
typedef Array<uint8_t>    ArrUByte;
typedef Array<int16_t>    ArrShort;
typedef Array<uint16_t>   ArrUShort;
typedef Array<int32_t>    ArrInt;
typedef Array<uint32_t>   ArrUInt;
typedef Array<int64_t>    ArrLong;
typedef Array<uint64_t>   ArrULong;
typedef Array<float>      ArrFloat;
typedef Array<double>     ArrDouble;

template <typename T>
class ArrayView;

typedef ArrayView<bool>       ArrViewBool;
typedef ArrayView<char>       ArrViewChar;
typedef ArrayView<int8_t>     ArrViewByte;
typedef ArrayView<uint8_t>    ArrViewUByte;
typedef ArrayView<int16_t>    ArrViewShort;
typedef ArrayView<uint16_t>   ArrViewUShort;
typedef ArrayView<int32_t>    ArrViewInt;
typedef ArrayView<uint32_t>   ArrViewUInt;
typedef ArrayView<int64_t>    ArrViewLong;
typedef ArrayView<uint64_t>   ArrViewULong;
typedef ArrayView<float>      ArrViewFloat;
typedef ArrayView<double>     ArrViewDouble;

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

}