// Wrapper for libwebsockets.h to suppress clang-21 module-import-in-extern-C warnings.
// libwebsockets.h wraps everything in extern "C" and then includes <stdint.h>/<limits.h>
// which clang 21 maps to built-in C++ module imports — forbidden inside extern "C".
#pragma once

#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wmodule-import-in-extern-c"
#endif
#include <libwebsockets.h>
#ifdef __clang__
# pragma clang diagnostic pop
#endif
