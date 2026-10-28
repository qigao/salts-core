/* SPDX-License-Identifier: Apache-2.0
 * Private first-party HKDF-SHA256/SHA384 key schedule for future native TLS13.
 * This file contains NO TLS session, record, I/O, fallback, or provider engine.
 * Compile in isolated first-party tests until the atomic native TLS cutover.
 */
#include "tls13_key_schedule.h"
#include "crypto_internal.h"
#include <crypto/sha256.h>
#include <crypto/sha512.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TLS13_MAX_LABEL_BYTES 249u
#define TLS13_MAX_CONTEXT_BYTES 255u
#define TLS13_MAX_INFO_BYTES (2u + 1u + 255u + 1u + TLS13_MAX_CONTEXT_BYTES)
#define TLS13_MAX_HASH_BYTES 48u
#define TLS13_MAX_HMAC_BLOCK_BYTES 128u

typedef union crypto_native_sha2_state {
    salts_crypto_sha256_ctx_t sha256;
    salts_crypto_sha384_ctx_t sha384;
} crypto_native_sha2_state;

static size_t hash_length(crypto_native_tls13_hash hash) {
    if (hash == CRYPTO_NATIVE_TLS13_SHA256) return 32u;
    if (hash == CRYPTO_NATIVE_TLS13_SHA384) return 48u;
    return 0u;
}

static size_t hash_block_length(crypto_native_tls13_hash hash) {
    if (hash == CRYPTO_NATIVE_TLS13_SHA256) return 64u;
    if (hash == CRYPTO_NATIVE_TLS13_SHA384) return 128u;
    return 0u;
}

static int sha_start(crypto_native_tls13_hash hash,
                     crypto_native_sha2_state *state) {
    return hash == CRYPTO_NATIVE_TLS13_SHA256
        ? salts_crypto_sha256_init(&state->sha256)
        : salts_crypto_sha384_init(&state->sha384);
}

static int sha_append(crypto_native_tls13_hash hash,
                      crypto_native_sha2_state *state,
                      const void *data, size_t length) {
    return hash == CRYPTO_NATIVE_TLS13_SHA256
        ? salts_crypto_sha256_update(&state->sha256, data, length)
        : salts_crypto_sha384_update(&state->sha384, data, length);
}

static int sha_finish(crypto_native_tls13_hash hash,
                      crypto_native_sha2_state *state, uint8_t *digest) {
    return hash == CRYPTO_NATIVE_TLS13_SHA256
        ? salts_crypto_sha256_final(&state->sha256, digest)
        : salts_crypto_sha384_final(&state->sha384, digest);
}

/* HMAC construction per RFC 2104, from ONE existing native SHA2 source.
 * Internal digest is copied to out only after every operation succeeds.
 * No caller-visible outputs are altered on a hash failure.
 */
static crypto_native_tls13_result hmac_sha2(
    crypto_native_tls13_hash hash, const uint8_t *key, size_t key_len,
    const uint8_t *p1, size_t n1, const uint8_t *p2, size_t n2,
    const uint8_t *p3, size_t n3, uint8_t *output) {
    uint8_t ipad[TLS13_MAX_HMAC_BLOCK_BYTES] = {0};
    uint8_t opad[TLS13_MAX_HMAC_BLOCK_BYTES] = {0};
    uint8_t inner[TLS13_MAX_HASH_BYTES] = {0};
    uint8_t compact_key[TLS13_MAX_HASH_BYTES] = {0};
    uint8_t outer[TLS13_MAX_HASH_BYTES] = {0};
    crypto_native_sha2_state state;
    crypto_native_tls13_result result = CRYPTO_NATIVE_TLS13_CRYPTO_ERROR;
    const size_t block_length = hash_block_length(hash);
    const size_t digest_length = hash_length(hash);

    memset(&state, 0, sizeof(state));
    if (!block_length || !digest_length || !output ||
        (!key && key_len) || (!p1 && n1) || (!p2 && n2) || (!p3 && n3))
        return CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT;

    if (key_len > block_length) {
        if (sha_start(hash, &state) != SALTS_CRYPTO_OK ||
            sha_append(hash, &state, key, key_len) != SALTS_CRYPTO_OK ||
            sha_finish(hash, &state, compact_key) != SALTS_CRYPTO_OK)
            goto finished;
        key = compact_key;
        key_len = digest_length;
    }
    if (key_len) memcpy(ipad, key, key_len);
    memcpy(opad, ipad, block_length);
    for (size_t i = 0u; i < block_length; ++i) {
        ipad[i] ^= 0x36u;
        opad[i] ^= 0x5cu;
    }
    if (sha_start(hash, &state) != SALTS_CRYPTO_OK ||
        sha_append(hash, &state, ipad, block_length) != SALTS_CRYPTO_OK ||
        sha_append(hash, &state, p1, n1) != SALTS_CRYPTO_OK ||
        sha_append(hash, &state, p2, n2) != SALTS_CRYPTO_OK ||
        sha_append(hash, &state, p3, n3) != SALTS_CRYPTO_OK ||
        sha_finish(hash, &state, inner) != SALTS_CRYPTO_OK)
        goto finished;

    if (sha_start(hash, &state) != SALTS_CRYPTO_OK ||
        sha_append(hash, &state, opad, block_length) != SALTS_CRYPTO_OK ||
        sha_append(hash, &state, inner, digest_length) != SALTS_CRYPTO_OK ||
        sha_finish(hash, &state, outer) != SALTS_CRYPTO_OK)
        goto finished;

    memcpy(output, outer, digest_length);
    result = CRYPTO_NATIVE_TLS13_OK;
finished:
    salts_crypto_secure_zero(&state, sizeof(state));
    salts_crypto_secure_zero(ipad, sizeof(ipad));
    salts_crypto_secure_zero(opad, sizeof(opad));
    salts_crypto_secure_zero(inner, sizeof(inner));
    salts_crypto_secure_zero(compact_key, sizeof(compact_key));
    salts_crypto_secure_zero(outer, sizeof(outer));
    return result;
}

crypto_native_tls13_result crypto_native_tls13_hkdf_extract(
    crypto_native_tls13_hash hash, const uint8_t *salt, size_t salt_len,
    const uint8_t *ikm, size_t ikm_len, uint8_t *out_prk,
    size_t out_capacity) {
    uint8_t zero_salt[TLS13_MAX_HASH_BYTES] = {0};
    uint8_t result[TLS13_MAX_HASH_BYTES] = {0};
    crypto_native_tls13_result status;
    const size_t digest_length = hash_length(hash);
    if (!digest_length || !out_prk || (!salt && salt_len) ||
        (!ikm && ikm_len))
        return CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT;
    if (out_capacity < digest_length)
        return CRYPTO_NATIVE_TLS13_RANGE;
    if (!salt_len) {
        salt = zero_salt;
        salt_len = digest_length;
    }
    status = hmac_sha2(hash, salt, salt_len, ikm, ikm_len,
                       NULL, 0u, NULL, 0u, result);
    if (status == CRYPTO_NATIVE_TLS13_OK)
        memcpy(out_prk, result, digest_length);
    salts_crypto_secure_zero(result, sizeof(result));
    salts_crypto_secure_zero(zero_salt, sizeof(zero_salt));
    return status;
}

crypto_native_tls13_result crypto_native_tls13_hkdf_expand_label(
    crypto_native_tls13_hash hash, const uint8_t *secret, size_t secret_len,
    const uint8_t *label, size_t label_len,
    const uint8_t *context, size_t context_len,
    uint8_t *output, size_t output_len) {
    static const uint8_t prefix[] = {'t','l','s','1','3',' '};
    uint8_t info[TLS13_MAX_INFO_BYTES] = {0};
    uint8_t previous[TLS13_MAX_HASH_BYTES] = {0};
    uint8_t digest[TLS13_MAX_HASH_BYTES] = {0};
    uint8_t message[TLS13_MAX_INFO_BYTES + TLS13_MAX_HASH_BYTES + 1u];
    uint8_t secret_copy[TLS13_MAX_HASH_BYTES] = {0};
    uint8_t *candidate = NULL;
    crypto_native_tls13_result status = CRYPTO_NATIVE_TLS13_CRYPTO_ERROR;
    size_t info_len = 0u, written = 0u, previous_len = 0u;
    size_t digest_length = hash_length(hash);

    if (!digest_length || secret_len != digest_length || !secret ||
        (label_len && !label) || (context_len && !context) ||
        (output_len && !output))
        return CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT;
    if (label_len > TLS13_MAX_LABEL_BYTES ||
        context_len > TLS13_MAX_CONTEXT_BYTES ||
        output_len > 65535u || output_len > digest_length * 255u)
        return CRYPTO_NATIVE_TLS13_RANGE;
    for (size_t i = 0u; i < label_len; ++i) {
        if (label[i] < 0x20u || label[i] > 0x7eu)
            return CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT;
    }

    info[info_len++] = (uint8_t)(output_len >> 8u);
    info[info_len++] = (uint8_t)output_len;
    info[info_len++] = (uint8_t)(sizeof(prefix) + label_len);
    memcpy(info + info_len, prefix, sizeof(prefix));
    info_len += sizeof(prefix);
    if (label_len) {
        memcpy(info + info_len, label, label_len);
        info_len += label_len;
    }
    info[info_len++] = (uint8_t)context_len;
    if (context_len) {
        memcpy(info + info_len, context, context_len);
        info_len += context_len;
    }
    /* Allow output to alias the secret or the borrowed context/label. */
    memcpy(secret_copy, secret, digest_length);
    if (!output_len) {
        status = CRYPTO_NATIVE_TLS13_OK;
        goto finished;
    }
    candidate = (uint8_t *)malloc(output_len);
    if (!candidate) {
        status = CRYPTO_NATIVE_TLS13_OUT_OF_MEMORY;
        goto finished;
    }
    for (uint32_t counter = 1u; written < output_len; ++counter) {
        size_t take = output_len - written;
        if (counter > 255u) {
            status = CRYPTO_NATIVE_TLS13_RANGE;
            goto finished;
        }
        if (take > digest_length) take = digest_length;
        if (previous_len) memcpy(message, previous, previous_len);
        memcpy(message + previous_len, info, info_len);
        message[previous_len + info_len] = (uint8_t)counter;
        status = hmac_sha2(hash, secret_copy, digest_length,
                           message, previous_len + info_len + 1u,
                           NULL, 0u, NULL, 0u, digest);
        if (status != CRYPTO_NATIVE_TLS13_OK) goto finished;
        memcpy(candidate + written, digest, take);
        memcpy(previous, digest, digest_length);
        previous_len = digest_length;
        written += take;
    }
    memcpy(output, candidate, output_len);
    status = CRYPTO_NATIVE_TLS13_OK;
finished:
    if (candidate) {
        salts_crypto_secure_zero(candidate, output_len);
        free(candidate);
    }
    salts_crypto_secure_zero(info, sizeof(info));
    salts_crypto_secure_zero(previous, sizeof(previous));
    salts_crypto_secure_zero(digest, sizeof(digest));
    salts_crypto_secure_zero(message, sizeof(message));
    salts_crypto_secure_zero(secret_copy, sizeof(secret_copy));
    return status;
}
