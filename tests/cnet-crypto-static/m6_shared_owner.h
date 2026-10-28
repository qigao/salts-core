/* SPDX-License-Identifier: Apache-2.0
 * TEST-ONLY CNet shared-Owner boundary. Never install in the public SDK.
 * Two independently loaded modules use this single DSO; neither embeds or
 * links the static Salts::Crypto archive. Opaque leases belong to the DSO.
 */
#ifndef CNET_M6_SHARED_OWNER_H
#define CNET_M6_SHARED_OWNER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Windows requires the declaration and definition of each exported
 * function to have matching linkage. Plugins import the same DLL; only
 * the candidate CNet DSO exports these test-only functions.
 */
#if defined(_WIN32)
#  if defined(CNET_M6_BUILD_SHARED)
#    define CNET_M6_API __declspec(dllexport)
#  else
#    define CNET_M6_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define CNET_M6_API __attribute__((visibility("default")))
#else
#  define CNET_M6_API
#endif

typedef struct cnet_m6_lease cnet_m6_lease;
CNET_M6_API int cnet_m6_shared_acquire(cnet_m6_lease **out_lease,
                           const void **out_owner, uint64_t *out_epoch);
CNET_M6_API int cnet_m6_shared_snapshot(const void **out_owner, uint32_t *out_leases,
                            uint32_t *out_sessions, uint64_t *out_epoch);
CNET_M6_API int cnet_m6_shared_release(cnet_m6_lease **lease);
/* Caller must hold a lease until this completes; no concurrent release. */
CNET_M6_API int cnet_m6_shared_native_smoke(cnet_m6_lease *lease);
/* Real original CNet/NativeIO TLS 1.2 or 1.3 mTLS with the Provider
 * borrowed from this sole DLL. The leased caller owns all file paths through
 * the call; the module NEVER persists a raw TLS or callback pointer.
 * expected_accept=0 demands exactly one handshake failure with no plaintext;
 * expected_accept=1 requires verified transport, payload, close and drain.
 */
CNET_M6_API int cnet_m6_shared_native_mtls(
    cnet_m6_lease *lease, uint32_t protocol,
    const char *ca, const char *server_cert, const char *server_key,
    const char *client_cert, const char *client_key, int expected_accept);

#ifdef __cplusplus
}
#endif
#endif
