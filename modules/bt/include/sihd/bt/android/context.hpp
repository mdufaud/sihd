#ifndef __SIHD_BT_ANDROID_CONTEXT_HPP__
#define __SIHD_BT_ANDROID_CONTEXT_HPP__

#include <jni.h>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_ANDROID__)
# error "sihd/bt/android headers are android only"
#endif

namespace sihd::bt::android_
{

// once, before any bluetooth call: the java side of sihd::bt loads through the class loader of this context
sihd::util::Result<void> set_context(JavaVM *vm, jobject context);

} // namespace sihd::bt::android_

#endif
