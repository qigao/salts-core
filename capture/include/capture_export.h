#ifndef SALTS_CAPTURE_EXPORT_H
#define SALTS_CAPTURE_EXPORT_H

#if defined(_WIN32) && defined(SALTS_CAPTURE_SHARED)
#  if defined(SALTS_CAPTURE_BUILD)
#    define SALTS_CAPTURE_API __declspec(dllexport)
#  else
#    define SALTS_CAPTURE_API __declspec(dllimport)
#  endif
#else
#  define SALTS_CAPTURE_API
#endif

#endif
