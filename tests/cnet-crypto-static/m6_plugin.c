/* SPDX-License-Identifier: Apache-2.0
 * Module A and B are built separately and link ONLY to the shared CNet
 * candidate. Neither imports nor compiles GmSSL or STATIC Salts::Crypto.
 */
#include "m6_shared_owner.h"
#include <cmeta_buffer.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#if defined(_WIN32)
#define M6_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define M6_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define M6_PLUGIN_EXPORT
#endif

M6_PLUGIN_EXPORT int m6_plugin_acquire(cnet_m6_lease **lease,
                                       const void **owner, uint64_t *epoch) {
    return cnet_m6_shared_acquire(lease, owner, epoch);
}
M6_PLUGIN_EXPORT int m6_plugin_release(cnet_m6_lease **lease) {
    return cnet_m6_shared_release(lease);
}
M6_PLUGIN_EXPORT int m6_plugin_snapshot(const void **owner, uint32_t *claims,
                                        uint32_t *sessions, uint64_t *epoch) {
    return cnet_m6_shared_snapshot(owner, claims, sessions, epoch);
}
M6_PLUGIN_EXPORT int m6_plugin_native_smoke(cnet_m6_lease *lease) {
    return cnet_m6_shared_native_smoke(lease);
}
M6_PLUGIN_EXPORT mem_pool_t *m6_plugin_core_pool(void) {
    return mem_global();
}
M6_PLUGIN_EXPORT const void *m6_plugin_thread_identity(void) {
    return cmeta_thread_current_token();
}
