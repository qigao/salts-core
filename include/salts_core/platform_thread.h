#ifndef SALTS_CORE_PLATFORM_THREAD_H
#define SALTS_CORE_PLATFORM_THREAD_H
#include <salts_core/platform_atomic.h>
#ifdef __cplusplus
extern "C" {
#endif

/* All native thread/lock/condition objects are opaque and module-owned.
 * No heap buffer or native pthread/HANDLE crosses the C compiler ABI.
 * Never copy owner handles or unload a callback's module while live.
 * Creation requires a NULL *out, and destruction/join/detach consumes it.
 */
typedef struct salts_platform_thread salts_platform_thread;
typedef struct salts_platform_mutex salts_platform_mutex;
typedef struct salts_platform_condition salts_platform_condition;
typedef int (SALTS_CORE_PLATFORM_CALL *salts_platform_thread_fn)(void *arg);
typedef void (SALTS_CORE_PLATFORM_CALL *salts_platform_once_fn)(void);
typedef struct salts_platform_once { salts_atomic32 state; } salts_platform_once;
#define SALTS_PLATFORM_ONCE_INIT { {0u} }

SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_thread_create(salts_platform_thread **out, salts_platform_thread_fn fn, void *arg);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_thread_join(salts_platform_thread **thread, int *result);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_thread_detach(salts_platform_thread **thread);
SALTS_CORE_PLATFORM_API void SALTS_CORE_PLATFORM_CALL salts_platform_thread_yield(void);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_call_once(salts_platform_once *flag, salts_platform_once_fn callback);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_create(salts_platform_mutex **out);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_lock(salts_platform_mutex *mutex);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_unlock(salts_platform_mutex *mutex);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_destroy(salts_platform_mutex **mutex);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_condition_create(salts_platform_condition **out);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_condition_wait(salts_platform_condition *condition, salts_platform_mutex *mutex);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_condition_signal(salts_platform_condition *condition);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_condition_broadcast(salts_platform_condition *condition);
SALTS_CORE_PLATFORM_API int SALTS_CORE_PLATFORM_CALL salts_platform_condition_destroy(salts_platform_condition **condition);
#ifdef __cplusplus
}
#endif
#endif
