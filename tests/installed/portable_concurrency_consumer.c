#include <salts_core/platform_thread.h>
#include <stddef.h>
#include <stdint.h>
static int worker(void *ctx) {
    return *(int *)ctx + 1;
}
int main(void) {
    salts_platform_thread *thread = NULL;
    salts_atomic64 slot = {0};
    int value = 41, result = 0;
    uint64_t observed = 0;
    struct aligncheck { char c; salts_atomic64 slot; };
    if (salts_platform_portable_abi_verify(SALTS_PLATFORM_PORTABLE_ABI_VERSION,
        sizeof(salts_atomic64), offsetof(struct aligncheck, slot),
        sizeof(salts_atomic_ptr))) return 1;
    if (salts_atomic64_init(&slot, 42ull) ||
        salts_atomic64_load(&slot, SALTS_MEMORY_ACQUIRE, &observed) ||
        observed != 42ull) return 2;
    if (salts_platform_thread_create(&thread, worker, &value) ||
        salts_platform_thread_join(&thread, &result) || result != 42 || thread)
        return 3;
    return 0;
}
