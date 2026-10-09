// Wrapper for libwebsockets.h: it wraps everything in extern "C" then includes <stdint.h>,
// which clang-21 maps to built-in C++ module imports - forbidden inside extern "C"
#pragma once

#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wmodule-import-in-extern-c"
#endif
#include <libwebsockets.h>
#ifdef __clang__
# pragma clang diagnostic pop
#endif
