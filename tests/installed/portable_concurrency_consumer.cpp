#include <salts_core/platform_atomic.h>
#include <salts_core/platform_thread.h>
#include <cstdint>
#include <cstddef>
#include <type_traits>
static_assert(std::is_standard_layout<salts_atomic64>::value, "C ABI only");
static_assert(sizeof(salts_atomic64) == 8, "64-bit ABI");
static_assert(alignof(salts_atomic64) >= 8, "64-bit alignment");
static int worker(void *arg) { return *static_cast<int *>(arg) + 9; }
int main() {
    salts_platform_thread *thread = nullptr;
    salts_atomic32 slot = {0};
    uint32_t observed = 0;
    int value = 33, result = 0;
    struct probe { char pad; salts_atomic64 slot; };
    if (salts_platform_portable_abi_verify(SALTS_PLATFORM_PORTABLE_ABI_VERSION,
        sizeof(salts_atomic64), offsetof(probe, slot),
        sizeof(salts_atomic_ptr))) return 1;
    if (salts_atomic32_init(&slot, 42) ||
        salts_atomic32_load(&slot, SALTS_MEMORY_ACQUIRE, &observed) ||
        observed != 42) return 2;
    if (salts_platform_thread_create(&thread, worker, &value) ||
        salts_platform_thread_join(&thread, &result) || result != 42 || thread) return 3;
    /* C++ consumes the exact same C ABI, never std::atomic storage. */
    salts_platform_tss *key = nullptr;
    void *got = nullptr;
    if (salts_platform_tss_create(&key, nullptr) ||
        salts_platform_tss_set(key, &value) ||
        salts_platform_tss_get(key, &got) || got != &value ||
        salts_platform_tss_set(key, nullptr) ||
        salts_platform_tss_destroy(&key) || key ||
        salts_platform_thread_sleep_ns(0))
        return 4;
    return 0;
}
