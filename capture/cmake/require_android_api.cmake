# The NDK legacy toolchain deliberately sets CMAKE_SYSTEM_VERSION to 1.
# Check the actual compiler API instead of interpreting that sentinel as an API.
include(CheckCSourceCompiles)
unset(SALTSCORE_CAPTURE_ANDROID_API_26)
unset(SALTSCORE_CAPTURE_ANDROID_API_26 CACHE)
check_c_source_compiles([=[
#include <android/api-level.h>
#if !defined(__ANDROID_API__) || __ANDROID_API__ < 26
#error Capture requires the actual Android compiler API to be at least 26
#endif
int main(void) { return 0; }
]=] SALTSCORE_CAPTURE_ANDROID_API_26)
if(NOT SALTSCORE_CAPTURE_ANDROID_API_26)
  message(FATAL_ERROR "The original Capture MediaProjection ABI requires Android API 26")
endif()
