#ifndef SALTS_PLAYBACK_EXPORT_H
#define SALTS_PLAYBACK_EXPORT_H

#if defined(_WIN32) && defined(SALTS_PLAYBACK_SHARED)
  #if defined(SALTS_PLAYBACK_BUILD)
    #define SALTS_PLAYBACK_API __declspec(dllexport)
  #else
    #define SALTS_PLAYBACK_API __declspec(dllimport)
  #endif
#elif defined(SALTS_PLAYBACK_SHARED) && (defined(__GNUC__) || defined(__clang__))
  #define SALTS_PLAYBACK_API __attribute__((visibility("default")))
#else
  #define SALTS_PLAYBACK_API
#endif

#endif /* SALTS_PLAYBACK_EXPORT_H */
