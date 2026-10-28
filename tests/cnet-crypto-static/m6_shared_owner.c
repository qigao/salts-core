/* SPDX-License-Identifier: Apache-2.0
 * TEST-ONLY sole physical Crypto Provider for the original shared CNet.
 * The static archive is linked ONLY to this DSO, never to either plugin
 * or the dlopen/LoadLibrary host. No production entry point is changed.
 */
#include "m6_shared_owner.h"
#include "public_context.h"
#include <crypto/tls.h>
#include <crypto/cipher_bio.h>
#include <cnet/cnet.h>
#include <salts/error_codes.h>
#include <salts/thread.h>
#include <salts/clock.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef SDK_CA_FILE
#error SDK_CA_FILE is mandatory for M6 real CNet profile
#endif

struct cnet_m6_lease {
    uint64_t epoch;
    const void *owner;
};
static atomic_flag owner_lock = ATOMIC_FLAG_INIT;
static unsigned char owner_identity;
static crypto_tls_provider *owner_provider;
static uint32_t owner_leases;
static uint64_t owner_epoch;
/* The TLS acceptance function is PRIVATE to the same shared candidate.
 * It is the original M5 two-client, real NativeIO test, but borrows this
 * one Provider instead of constructing and destroying a second Provider.
 */
extern int cnet_m6_original_owner_mtls_case(
    crypto_tls_provider *shared_provider, uint32_t protocol,
    const char *ca, const char *server_cert, const char *server_key,
    const char *client_cert, const char *client_key, bool expect_accept);

/* Two separately loaded plugins must enter their original CNet tests at
 * the same time. The odd/even admission gates are bounded and never retry
 * TLS. This is test-only coordination, not a runtime policy/fallback.
 */
static atomic_uint m6_mtls_arrivals = 0u;


static int lock_owner(void)
{
    unsigned spin;
    for (spin = 0; spin != 200000u; ++spin) {
        if (!atomic_flag_test_and_set_explicit(&owner_lock,
                                                memory_order_acquire))
            return SALTS_OK;
        cmeta_thread_yield();
    }
    return SALTS_EBUSY;
}
static void unlock_owner(void)
{
    atomic_flag_clear_explicit(&owner_lock, memory_order_release);
}

CNET_M6_API int cnet_m6_shared_acquire(cnet_m6_lease **out_lease,
                                     const void **out_owner,
                                     uint64_t *out_epoch)
{
    cnet_m6_lease *lease;
    int status;
    if (!out_lease || !out_owner || !out_epoch || *out_lease != NULL)
        return SALTS_EINVAL;
    status = lock_owner();
    if (status != SALTS_OK) return status;
    if (owner_provider == NULL) {
        if (crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION ||
            crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION,
                                       &owner_provider) != CRYPTO_TLS_OK) {
            unlock_owner();
            return SALTS_EIO;
        }
        ++owner_epoch;
    }
    if (owner_leases == UINT32_MAX) {
        unlock_owner();
        return SALTS_ENOBUFS;
    }
    lease = (cnet_m6_lease *)calloc(1u, sizeof(*lease));
    if (!lease) {
        /* Leave a freshly opened provider with no claims closed. */
        if (owner_leases == 0u)
            (void)crypto_tls_provider_destroy(&owner_provider);
        unlock_owner();
        return SALTS_ENOMEM;
    }
    lease->owner = &owner_identity;
    lease->epoch = owner_epoch;
    ++owner_leases;
    *out_lease = lease;
    *out_owner = lease->owner;
    *out_epoch = lease->epoch;
    unlock_owner();
    return SALTS_OK;
}

CNET_M6_API int cnet_m6_shared_snapshot(const void **out_owner,
                                      uint32_t *out_leases,
                                      uint32_t *out_sessions,
                                      uint64_t *out_epoch)
{
    int status;
    uint32_t count = 0u;
    if (!out_owner || !out_leases || !out_sessions || !out_epoch)
        return SALTS_EINVAL;
    status = lock_owner();
    if (status != SALTS_OK) return status;
    if (owner_provider &&
        crypto_tls_provider_session_count(owner_provider, &count) != CRYPTO_TLS_OK) {
        unlock_owner();
        return SALTS_EIO;
    }
    *out_owner = &owner_identity;
    *out_leases = owner_leases;
    *out_sessions = count;
    *out_epoch = owner_epoch;
    unlock_owner();
    return SALTS_OK;
}

CNET_M6_API int cnet_m6_shared_release(cnet_m6_lease **in_out)
{
    int status;
    cnet_m6_lease *lease;
    if (!in_out || !*in_out) return SALTS_EINVAL;
    status = lock_owner();
    if (status != SALTS_OK) return status;
    lease = *in_out;
    if (lease->owner != &owner_identity || lease->epoch != owner_epoch ||
        !owner_provider || !owner_leases) {
        unlock_owner();
        return SALTS_EINVAL;
    }
    if (owner_leases == 1u &&
        crypto_tls_provider_destroy(&owner_provider) != CRYPTO_TLS_OK) {
        /* In-flight NativeIO/TLS sessions must finish first. Never destroy
         * the owner or mark a lease released while Provider says BUSY. */
        unlock_owner();
        return SALTS_EBUSY;
    }
    --owner_leases;
    *in_out = NULL;
    unlock_owner();
    free(lease);
    return SALTS_OK;
}

CNET_M6_API int cnet_m6_shared_native_smoke(cnet_m6_lease *lease)
{
    cnet_client client = {0};
    cnet_client_config cfg = {0};
    cnet_tls_client tls = {0};
    cnet_tls_client_config policy = {0};
    crypto_tls_provider *provider;
    int status, live = 0;
    if (!lease) return SALTS_EINVAL;
    status = lock_owner();
    if (status != SALTS_OK) return status;
    if (lease->owner != &owner_identity || lease->epoch != owner_epoch ||
        owner_leases == 0u || owner_provider == NULL) {
        unlock_owner();
        return SALTS_EINVAL;
    }
    provider = owner_provider;  /* lease retained until smoke returns */
    unlock_owner();

    policy.size = sizeof(policy);
    policy.security_profile = CNET_TLS_SECURITY_PROFILE_STANDARD;
    policy.ca_file = SDK_CA_FILE;
    policy.server_name = "localhost";
    status = cnet_crypto_test_bind_client(
        &tls, provider, &policy, "localhost", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY);
    if (status != SALTS_OK) return status;
#if defined(_WIN32)
    cfg.backend = NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
    cfg.backend = NATIVE_IO_BACKEND_KQUEUE;
#else
    cfg.backend = NATIVE_IO_BACKEND_EPOLL;
#endif
    cfg.connection_capacity = 2u;
    cfg.command_capacity = 8u;
    cfg.request_capacity = 8u;
    cfg.completion_batch_capacity = 8u;
    cfg.event_capacity = 8u;
    cfg.max_send_bytes = 1024u;
    cfg.receive_buffer_bytes = 1024u;
    cfg.tls_io_buffer_bytes = CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY;
    /* The original public CNet validator requires TLS buffer and handshake
     * deadline to be supplied together; a half-configured fixture is EINVAL.
     * Keep the real native timeout boundary, never weaken validation.
     */
    cfg.tls_handshake_timeout_ms = 15000u;
    status = cnet_client_init(&client, &cfg);
    if (status != SALTS_OK) goto done;
    live = 1;
    status = cnet_client_stop(&client, 3000u);
    if (status == SALTS_OK)
        status = cnet_client_destroy(&client);
    if (status == SALTS_OK) live = 0;
done:
    if (live) {
        (void)cnet_client_stop(&client, 3000u);
        (void)cnet_client_destroy(&client);
    }
    if (cnet_tls_client_destroy(&tls) != SALTS_OK)
        return SALTS_EIO;
    return status;
}

CNET_M6_API int cnet_m6_shared_native_mtls(
    cnet_m6_lease *lease, uint32_t protocol,
    const char *ca, const char *server_cert, const char *server_key,
    const char *client_cert, const char *client_key, int expected_accept)
{
    crypto_tls_provider *shared;
    int status;
    unsigned ticket;
    if (!lease || !ca || !server_cert || !server_key ||
        ((client_cert == NULL) != (client_key == NULL)) ||
        (protocol != CRYPTO_TLS_1_2 && protocol != CRYPTO_TLS_1_3) ||
        (expected_accept != 0 && expected_accept != 1))
        return SALTS_EINVAL;
    status = lock_owner();
    if (status != SALTS_OK) return status;
    if (lease->owner != &owner_identity ||
        lease->epoch != owner_epoch || owner_provider == NULL ||
        owner_leases == 0u) {
        unlock_owner();
        return SALTS_EINVAL;
    }
    /* Caller retains lease; no Provider destruction before this returns. */
    shared = owner_provider;
    unlock_owner();

    ticket = atomic_fetch_add_explicit(
        &m6_mtls_arrivals, 1u, memory_order_acq_rel);
    if ((ticket & 1u) == 0u) {
        const uint64_t deadline = cmeta_monotonic_ms() + UINT64_C(5000);
        while (atomic_load_explicit(&m6_mtls_arrivals,
                                    memory_order_acquire) <= ticket) {
            if (cmeta_monotonic_ms() >= deadline)
                return SALTS_ETIMEDOUT;
            cmeta_thread_yield();
        }
    }
    return cnet_m6_original_owner_mtls_case(
        shared, protocol, ca, server_cert, server_key, client_cert,
        client_key, expected_accept != 0);
}
