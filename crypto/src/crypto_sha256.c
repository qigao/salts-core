/* SPDX-License-Identifier: Apache-2.0
 * First-party C11 SHA-256 (FIPS 180-4), owned only by Salts::Crypto.
 *
 * No GmSSL, libecc, OpenSSL, platform TLS, heap state, shared globals or
 * runtime provider selection. Keep the original public 128-byte opaque
 * context ABI; all intermediate state belongs to the caller-owned context.
 */
#include <crypto/sha256.h>
#include "crypto_internal.h"

#include <stdint.h>
#include <string.h>

#define SALTS_SHA256_BLOCK_BYTES 64u
#define SALTS_SHA256_MAGIC UINT32_C(0x53733235)

typedef struct salts_crypto_sha256_impl {
    uint64_t total_bytes;
    uint32_t state[8];
    uint8_t block[SALTS_SHA256_BLOCK_BYTES];
    uint32_t used;
    uint32_t magic;
} salts_crypto_sha256_impl;

_Static_assert(sizeof(salts_crypto_sha256_impl) <= SALTS_CRYPTO_SHA256_CONTEXT_SIZE,
               "native SHA256 exceeds public opaque context ABI");
_Static_assert(_Alignof(salts_crypto_sha256_impl) <= _Alignof(salts_crypto_sha256_ctx_t),
               "native SHA256 has stricter alignment than public ABI");

static salts_crypto_sha256_impl *sha_state(salts_crypto_sha256_ctx_t *ctx) {
    return (salts_crypto_sha256_impl *)(void *)ctx->bytes;
}

static uint32_t rotr32(uint32_t value, unsigned count) {
    return (value >> count) | (value << (32u - count));
}

static uint32_t big_endian_u32(const uint8_t bytes[4]) {
    return ((uint32_t)bytes[0] << 24u) |
           ((uint32_t)bytes[1] << 16u) |
           ((uint32_t)bytes[2] << 8u) |
           ((uint32_t)bytes[3]);
}

static void save_big_endian_u32(uint8_t output[4], uint32_t value) {
    output[0] = (uint8_t)(value >> 24u);
    output[1] = (uint8_t)(value >> 16u);
    output[2] = (uint8_t)(value >> 8u);
    output[3] = (uint8_t)value;
}

static void compress(salts_crypto_sha256_impl *ctx,
                     const uint8_t block[SALTS_SHA256_BLOCK_BYTES]) {
    static const uint32_t rounds[64] = {
        UINT32_C(0x428a2f98),UINT32_C(0x71374491),UINT32_C(0xb5c0fbcf),UINT32_C(0xe9b5dba5),
        UINT32_C(0x3956c25b),UINT32_C(0x59f111f1),UINT32_C(0x923f82a4),UINT32_C(0xab1c5ed5),
        UINT32_C(0xd807aa98),UINT32_C(0x12835b01),UINT32_C(0x243185be),UINT32_C(0x550c7dc3),
        UINT32_C(0x72be5d74),UINT32_C(0x80deb1fe),UINT32_C(0x9bdc06a7),UINT32_C(0xc19bf174),
        UINT32_C(0xe49b69c1),UINT32_C(0xefbe4786),UINT32_C(0x0fc19dc6),UINT32_C(0x240ca1cc),
        UINT32_C(0x2de92c6f),UINT32_C(0x4a7484aa),UINT32_C(0x5cb0a9dc),UINT32_C(0x76f988da),
        UINT32_C(0x983e5152),UINT32_C(0xa831c66d),UINT32_C(0xb00327c8),UINT32_C(0xbf597fc7),
        UINT32_C(0xc6e00bf3),UINT32_C(0xd5a79147),UINT32_C(0x06ca6351),UINT32_C(0x14292967),
        UINT32_C(0x27b70a85),UINT32_C(0x2e1b2138),UINT32_C(0x4d2c6dfc),UINT32_C(0x53380d13),
        UINT32_C(0x650a7354),UINT32_C(0x766a0abb),UINT32_C(0x81c2c92e),UINT32_C(0x92722c85),
        UINT32_C(0xa2bfe8a1),UINT32_C(0xa81a664b),UINT32_C(0xc24b8b70),UINT32_C(0xc76c51a3),
        UINT32_C(0xd192e819),UINT32_C(0xd6990624),UINT32_C(0xf40e3585),UINT32_C(0x106aa070),
        UINT32_C(0x19a4c116),UINT32_C(0x1e376c08),UINT32_C(0x2748774c),UINT32_C(0x34b0bcb5),
        UINT32_C(0x391c0cb3),UINT32_C(0x4ed8aa4a),UINT32_C(0x5b9cca4f),UINT32_C(0x682e6ff3),
        UINT32_C(0x748f82ee),UINT32_C(0x78a5636f),UINT32_C(0x84c87814),UINT32_C(0x8cc70208),
        UINT32_C(0x90befffa),UINT32_C(0xa4506ceb),UINT32_C(0xbef9a3f7),UINT32_C(0xc67178f2)
    };
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    size_t i;
    for (i = 0u; i < 16u; ++i)
        w[i] = big_endian_u32(block + i * 4u);
    for (i = 16u; i < 64u; ++i) {
        const uint32_t x = w[i - 15u], y = w[i - 2u];
        const uint32_t s0 = rotr32(x, 7u) ^ rotr32(x, 18u) ^ (x >> 3u);
        const uint32_t s1 = rotr32(y, 17u) ^ rotr32(y, 19u) ^ (y >> 10u);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0u; i < 64u; ++i) {
        const uint32_t e1 = rotr32(e, 6u) ^ rotr32(e, 11u) ^ rotr32(e, 25u);
        const uint32_t a0 = rotr32(a, 2u) ^ rotr32(a, 13u) ^ rotr32(a, 22u);
        const uint32_t t1 = h + e1 + ((e & f) ^ ((~e) & g)) + rounds[i] + w[i];
        const uint32_t t2 = a0 + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
    salts_crypto_secure_zero(w, sizeof(w));
}

int salts_crypto_sha256_init(salts_crypto_sha256_ctx_t *context) {
    static const uint32_t initial[8] = {
        UINT32_C(0x6a09e667),UINT32_C(0xbb67ae85),UINT32_C(0x3c6ef372),UINT32_C(0xa54ff53a),
        UINT32_C(0x510e527f),UINT32_C(0x9b05688c),UINT32_C(0x1f83d9ab),UINT32_C(0x5be0cd19)
    };
    salts_crypto_sha256_impl *impl;
    if (!context) return SALTS_CRYPTO_EINVAL;
    memset(context, 0, sizeof(*context));
    impl = sha_state(context);
    memcpy(impl->state, initial, sizeof(initial));
    impl->magic = SALTS_SHA256_MAGIC;
    return SALTS_CRYPTO_OK;
}

int salts_crypto_sha256_update(salts_crypto_sha256_ctx_t *context,
                               const void *data, size_t data_size) {
    salts_crypto_sha256_impl *impl;
    const uint8_t *cursor = (const uint8_t *)data;
    size_t remaining = data_size;
    if (!context || (!data && data_size != 0u)) return SALTS_CRYPTO_EINVAL;
    impl = sha_state(context);
    if (impl->magic != SALTS_SHA256_MAGIC) return SALTS_CRYPTO_ESTATE;
    /* FIPS 180-4 encodes the message bit length in exactly 64 bits.
     * Reject overflow before modifying any state (no length wraparound).
     */
    if (data_size > UINT64_MAX / 8u - impl->total_bytes)
        return SALTS_CRYPTO_EINVAL;
    while (remaining) {
        size_t take = (size_t)SALTS_SHA256_BLOCK_BYTES - impl->used;
        if (take > remaining) take = remaining;
        memcpy(impl->block + impl->used, cursor, take);
        cursor += take;
        impl->used += (uint32_t)take;
        remaining -= take;
        if (impl->used == SALTS_SHA256_BLOCK_BYTES) {
            compress(impl, impl->block);
            impl->used = 0u;
        }
    }
    impl->total_bytes += data_size;
    return SALTS_CRYPTO_OK;
}

int salts_crypto_sha256_final(salts_crypto_sha256_ctx_t *context,
                              uint8_t digest[SALTS_CRYPTO_SHA256_DIGEST_SIZE]) {
    salts_crypto_sha256_impl *impl;
    uint64_t length_bits;
    size_t i;
    if (!context || !digest) return SALTS_CRYPTO_EINVAL;
    impl = sha_state(context);
    if (impl->magic != SALTS_SHA256_MAGIC) return SALTS_CRYPTO_ESTATE;

    length_bits = impl->total_bytes * UINT64_C(8);
    impl->block[impl->used++] = 0x80u;
    if (impl->used > 56u) {
        memset(impl->block + impl->used, 0, SALTS_SHA256_BLOCK_BYTES - impl->used);
        compress(impl, impl->block);
        impl->used = 0u;
    }
    memset(impl->block + impl->used, 0, 56u - impl->used);
    for (i = 0u; i < 8u; ++i)
        impl->block[56u + i] = (uint8_t)(length_bits >> (56u - i * 8u));
    compress(impl, impl->block);
    for (i = 0u; i < 8u; ++i)
        save_big_endian_u32(digest + i * 4u, impl->state[i]);
    salts_crypto_secure_zero(impl, sizeof(*impl));
    return SALTS_CRYPTO_OK;
}

int salts_crypto_sha256(const void *data, size_t data_size,
                        uint8_t digest[SALTS_CRYPTO_SHA256_DIGEST_SIZE]) {
    salts_crypto_sha256_ctx_t context;
    int status;
    if (!digest || (!data && data_size != 0u)) return SALTS_CRYPTO_EINVAL;
    status = salts_crypto_sha256_init(&context);
    if (status == SALTS_CRYPTO_OK)
        status = salts_crypto_sha256_update(&context, data, data_size);
    if (status == SALTS_CRYPTO_OK)
        status = salts_crypto_sha256_final(&context, digest);
    salts_crypto_secure_zero(&context, sizeof(context));
    if (status != SALTS_CRYPTO_OK)
        salts_crypto_secure_zero(digest, SALTS_CRYPTO_SHA256_DIGEST_SIZE);
    return status;
}
