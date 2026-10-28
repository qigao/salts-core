/* SPDX-License-Identifier: Apache-2.0
 * First-party C11 SHA-256 (FIPS 180-4), owned only by Salts::Crypto.
 *
 * No GmSSL, libecc, OpenSSL, platform TLS, heap state, shared globals or
 * runtime provider selection. Keep the original public 128-byte opaque
 * context ABI; all intermediate state belongs to the caller-owned context.
 */
#include <crypto/sha256.h>
#include <crypto/sha224.h>
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

/* The public ABI is a union with an unsigned-byte buffer. Casting that
 * buffer to a struct pointer is not guaranteed by strict-aliasing C11
 * rules, even when alignment is sufficient. Use memcpy to materialize a
 * properly typed local state, then copy it back; never type-pun memory.
 * All local state is wiped before returning, including failure paths.
 */
static void sha_load(const salts_crypto_sha256_ctx_t *ctx,
                     salts_crypto_sha256_impl *state) {
    memcpy(state, ctx->bytes, sizeof(*state));
}
static void sha_save(salts_crypto_sha256_ctx_t *ctx,
                     const salts_crypto_sha256_impl *state) {
    memcpy(ctx->bytes, state, sizeof(*state));
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
    salts_crypto_sha256_impl state = {0};
    if (!context) return SALTS_CRYPTO_EINVAL;
    memset(context, 0, sizeof(*context));
    memcpy(state.state, initial, sizeof(initial));
    state.magic = SALTS_SHA256_MAGIC;
    sha_save(context, &state);
    salts_crypto_secure_zero(&state, sizeof(state));
    return SALTS_CRYPTO_OK;
}

int salts_crypto_sha256_update(salts_crypto_sha256_ctx_t *context,
                               const void *data, size_t data_size) {
    salts_crypto_sha256_impl local;
    const uint8_t *cursor = (const uint8_t *)data;
    size_t remaining = data_size;
    if (!context || (!data && data_size != 0u)) return SALTS_CRYPTO_EINVAL;
    sha_load(context, &local);
    if (local.magic != SALTS_SHA256_MAGIC) {
        salts_crypto_secure_zero(&local, sizeof(local));
        return SALTS_CRYPTO_ESTATE;
    }
    /* FIPS 180-4 encodes the message bit length in exactly 64 bits.
     * Reject overflow before modifying any state (no length wraparound).
     */
    if (data_size > UINT64_MAX / 8u - local.total_bytes) {
        salts_crypto_secure_zero(&local, sizeof(local));
        return SALTS_CRYPTO_EINVAL;
    }
    while (remaining) {
        size_t take = (size_t)SALTS_SHA256_BLOCK_BYTES - local.used;
        if (take > remaining) take = remaining;
        memcpy(local.block + local.used, cursor, take);
        cursor += take;
        local.used += (uint32_t)take;
        remaining -= take;
        if (local.used == SALTS_SHA256_BLOCK_BYTES) {
            compress(&local, local.block);
            local.used = 0u;
        }
    }
    local.total_bytes += data_size;
    sha_save(context, &local);
    salts_crypto_secure_zero(&local, sizeof(local));
    return SALTS_CRYPTO_OK;
}

int salts_crypto_sha256_final(salts_crypto_sha256_ctx_t *context,
                              uint8_t digest[SALTS_CRYPTO_SHA256_DIGEST_SIZE]) {
    salts_crypto_sha256_impl local;
    uint64_t length_bits;
    size_t i;
    if (!context || !digest) return SALTS_CRYPTO_EINVAL;
    sha_load(context, &local);
    if (local.magic != SALTS_SHA256_MAGIC) {
        salts_crypto_secure_zero(&local, sizeof(local));
        return SALTS_CRYPTO_ESTATE;
    }

    length_bits = local.total_bytes * UINT64_C(8);
    local.block[local.used++] = 0x80u;
    if (local.used > 56u) {
        memset(local.block + local.used, 0, SALTS_SHA256_BLOCK_BYTES - local.used);
        compress(&local, local.block);
        local.used = 0u;
    }
    memset(local.block + local.used, 0, 56u - local.used);
    for (i = 0u; i < 8u; ++i)
        local.block[56u + i] = (uint8_t)(length_bits >> (56u - i * 8u));
    compress(&local, local.block);
    for (i = 0u; i < 8u; ++i)
        save_big_endian_u32(digest + i * 4u, local.state[i]);
    salts_crypto_secure_zero(context, sizeof(*context));
    salts_crypto_secure_zero(&local, sizeof(local));
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

/* SHA-224 is the same FIPS 180-4 32-bit compression as SHA-256, but
 * with its own eight initial values and 28-byte truncated final digest.
 * The canonical SHA256 update/final already enforce bit-length bounds,
 * clean intermediate state, and use alias-safe typed local state.
 */
int salts_crypto_sha224(const void *data, size_t data_size,
                        uint8_t digest[SALTS_CRYPTO_SHA224_DIGEST_SIZE]) {
    static const uint32_t iv[8] = {
        UINT32_C(0xc1059ed8),UINT32_C(0x367cd507),
        UINT32_C(0x3070dd17),UINT32_C(0xf70e5939),
        UINT32_C(0xffc00b31),UINT32_C(0x68581511),
        UINT32_C(0x64f98fa7),UINT32_C(0xbefa4fa4)
    };
    salts_crypto_sha256_ctx_t context;
    salts_crypto_sha256_impl local = {0};
    uint8_t full[SALTS_CRYPTO_SHA256_DIGEST_SIZE] = {0};
    int result;
    if (digest == NULL || (data == NULL && data_size != 0u))
        return SALTS_CRYPTO_EINVAL;
    memset(&context, 0, sizeof(context));
    memcpy(local.state, iv, sizeof(iv));
    local.magic = SALTS_SHA256_MAGIC;
    sha_save(&context, &local);
    salts_crypto_secure_zero(&local, sizeof(local));
    result = salts_crypto_sha256_update(&context, data, data_size);
    if (result == SALTS_CRYPTO_OK)
        result = salts_crypto_sha256_final(&context, full);
    if (result == SALTS_CRYPTO_OK)
        memcpy(digest, full, SALTS_CRYPTO_SHA224_DIGEST_SIZE);
    else
        salts_crypto_secure_zero(digest, SALTS_CRYPTO_SHA224_DIGEST_SIZE);
    salts_crypto_secure_zero(full, sizeof(full));
    salts_crypto_secure_zero(&context, sizeof(context));
    return result;
}


/* Native SHA384/512 uses one 64-bit FIPS 180-4 compressor, NOT the
 * GmSSL TLS/record engine. Staged in the existing SHA2 compilation unit
 * to preserve the immutable imported Crypto/CMakeLists.txt provenance.
 */
#include <crypto/sha512.h>
#define SALTS_SHA512_BLOCK_BYTES 128u
#define SALTS_SHA512_MAGIC UINT32_C(0x53353132)
#define SALTS_SHA384_MAGIC UINT32_C(0x53333834)
typedef struct salts_crypto_sha512_impl {
    uint64_t length_high, length_low; /* 128-bit message length in bytes */
    uint64_t state[8];
    uint8_t block[SALTS_SHA512_BLOCK_BYTES];
    uint32_t used, magic;
} salts_crypto_sha512_impl;
_Static_assert(sizeof(salts_crypto_sha512_impl) <= SALTS_CRYPTO_SHA512_CONTEXT_SIZE,
               "SHA512 state exceeds public opaque context");
_Static_assert(_Alignof(salts_crypto_sha512_impl) <= _Alignof(salts_crypto_sha512_ctx_t),
               "SHA512 state exceeds public context alignment");

static uint64_t sha512_rotr(uint64_t x, unsigned n) {
    return (x >> n) | (x << (64u - n));
}
static uint64_t sha512_load64(const uint8_t *p) {
    uint64_t value = 0u;
    for (unsigned i = 0u; i < 8u; ++i) value = (value << 8u) | p[i];
    return value;
}
static void sha512_store64(uint8_t *p, uint64_t value) {
    for (unsigned i = 0u; i < 8u; ++i)
        p[i] = (uint8_t)(value >> (56u - 8u * i));
}
static void sha512_compress(salts_crypto_sha512_impl *s,
                            const uint8_t block[SALTS_SHA512_BLOCK_BYTES]) {
    static const uint64_t rounds[80] = {
        UINT64_C(0x428a2f98d728ae22), UINT64_C(0x7137449123ef65cd), UINT64_C(0xb5c0fbcfec4d3b2f), UINT64_C(0xe9b5dba58189dbbc),
        UINT64_C(0x3956c25bf348b538), UINT64_C(0x59f111f1b605d019), UINT64_C(0x923f82a4af194f9b), UINT64_C(0xab1c5ed5da6d8118),
        UINT64_C(0xd807aa98a3030242), UINT64_C(0x12835b0145706fbe), UINT64_C(0x243185be4ee4b28c), UINT64_C(0x550c7dc3d5ffb4e2),
        UINT64_C(0x72be5d74f27b896f), UINT64_C(0x80deb1fe3b1696b1), UINT64_C(0x9bdc06a725c71235), UINT64_C(0xc19bf174cf692694),
        UINT64_C(0xe49b69c19ef14ad2), UINT64_C(0xefbe4786384f25e3), UINT64_C(0x0fc19dc68b8cd5b5), UINT64_C(0x240ca1cc77ac9c65),
        UINT64_C(0x2de92c6f592b0275), UINT64_C(0x4a7484aa6ea6e483), UINT64_C(0x5cb0a9dcbd41fbd4), UINT64_C(0x76f988da831153b5),
        UINT64_C(0x983e5152ee66dfab), UINT64_C(0xa831c66d2db43210), UINT64_C(0xb00327c898fb213f), UINT64_C(0xbf597fc7beef0ee4),
        UINT64_C(0xc6e00bf33da88fc2), UINT64_C(0xd5a79147930aa725), UINT64_C(0x06ca6351e003826f), UINT64_C(0x142929670a0e6e70),
        UINT64_C(0x27b70a8546d22ffc), UINT64_C(0x2e1b21385c26c926), UINT64_C(0x4d2c6dfc5ac42aed), UINT64_C(0x53380d139d95b3df),
        UINT64_C(0x650a73548baf63de), UINT64_C(0x766a0abb3c77b2a8), UINT64_C(0x81c2c92e47edaee6), UINT64_C(0x92722c851482353b),
        UINT64_C(0xa2bfe8a14cf10364), UINT64_C(0xa81a664bbc423001), UINT64_C(0xc24b8b70d0f89791), UINT64_C(0xc76c51a30654be30),
        UINT64_C(0xd192e819d6ef5218), UINT64_C(0xd69906245565a910), UINT64_C(0xf40e35855771202a), UINT64_C(0x106aa07032bbd1b8),
        UINT64_C(0x19a4c116b8d2d0c8), UINT64_C(0x1e376c085141ab53), UINT64_C(0x2748774cdf8eeb99), UINT64_C(0x34b0bcb5e19b48a8),
        UINT64_C(0x391c0cb3c5c95a63), UINT64_C(0x4ed8aa4ae3418acb), UINT64_C(0x5b9cca4f7763e373), UINT64_C(0x682e6ff3d6b2b8a3),
        UINT64_C(0x748f82ee5defb2fc), UINT64_C(0x78a5636f43172f60), UINT64_C(0x84c87814a1f0ab72), UINT64_C(0x8cc702081a6439ec),
        UINT64_C(0x90befffa23631e28), UINT64_C(0xa4506cebde82bde9), UINT64_C(0xbef9a3f7b2c67915), UINT64_C(0xc67178f2e372532b),
        UINT64_C(0xca273eceea26619c), UINT64_C(0xd186b8c721c0c207), UINT64_C(0xeada7dd6cde0eb1e), UINT64_C(0xf57d4f7fee6ed178),
        UINT64_C(0x06f067aa72176fba), UINT64_C(0x0a637dc5a2c898a6), UINT64_C(0x113f9804bef90dae), UINT64_C(0x1b710b35131c471b),
        UINT64_C(0x28db77f523047d84), UINT64_C(0x32caab7b40c72493), UINT64_C(0x3c9ebe0a15c9bebc), UINT64_C(0x431d67c49c100d4c),
        UINT64_C(0x4cc5d4becb3e42b6), UINT64_C(0x597f299cfc657e2a), UINT64_C(0x5fcb6fab3ad6faec), UINT64_C(0x6c44198c4a475817)
    };
    uint64_t w[80], a,b,c,d,e,f,g,h;
    for (unsigned i = 0u; i < 16u; ++i)
        w[i] = sha512_load64(block + i * 8u);
    for (unsigned i = 16u; i < 80u; ++i) {
        const uint64_t x = w[i - 15u], y = w[i - 2u];
        const uint64_t sigma0 = sha512_rotr(x,1u) ^ sha512_rotr(x,8u) ^ (x >> 7u);
        const uint64_t sigma1 = sha512_rotr(y,19u) ^ sha512_rotr(y,61u) ^ (y >> 6u);
        w[i] = w[i - 16u] + sigma0 + w[i - 7u] + sigma1;
    }
    a=s->state[0]; b=s->state[1]; c=s->state[2]; d=s->state[3];
    e=s->state[4]; f=s->state[5]; g=s->state[6]; h=s->state[7];
    for (unsigned i = 0u; i < 80u; ++i) {
        const uint64_t s1 = sha512_rotr(e,14u) ^ sha512_rotr(e,18u) ^ sha512_rotr(e,41u);
        const uint64_t s0 = sha512_rotr(a,28u) ^ sha512_rotr(a,34u) ^ sha512_rotr(a,39u);
        const uint64_t t1 = h + s1 + ((e & f) ^ ((~e) & g)) + rounds[i] + w[i];
        const uint64_t t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    s->state[0]+=a; s->state[1]+=b; s->state[2]+=c; s->state[3]+=d;
    s->state[4]+=e; s->state[5]+=f; s->state[6]+=g; s->state[7]+=h;
    salts_crypto_secure_zero(w,sizeof(w));
}
static int sha512_start(salts_crypto_sha512_ctx_t *ctx, uint32_t variant) {
    static const uint64_t iv512[8]={
        UINT64_C(0x6a09e667f3bcc908),UINT64_C(0xbb67ae8584caa73b),
        UINT64_C(0x3c6ef372fe94f82b),UINT64_C(0xa54ff53a5f1d36f1),
        UINT64_C(0x510e527fade682d1),UINT64_C(0x9b05688c2b3e6c1f),
        UINT64_C(0x1f83d9abfb41bd6b),UINT64_C(0x5be0cd19137e2179)};
    static const uint64_t iv384[8]={
        UINT64_C(0xcbbb9d5dc1059ed8),UINT64_C(0x629a292a367cd507),
        UINT64_C(0x9159015a3070dd17),UINT64_C(0x152fecd8f70e5939),
        UINT64_C(0x67332667ffc00b31),UINT64_C(0x8eb44a8768581511),
        UINT64_C(0xdb0c2e0d64f98fa7),UINT64_C(0x47b5481dbefa4fa4)};
    salts_crypto_sha512_impl local={0};
    if (!ctx) return SALTS_CRYPTO_EINVAL;
    memset(ctx,0,sizeof(*ctx));
    memcpy(local.state, variant == SALTS_SHA384_MAGIC ? iv384 : iv512,
           sizeof(local.state));
    local.magic=variant;
    memcpy(ctx->bytes,&local,sizeof(local));
    salts_crypto_secure_zero(&local,sizeof(local));
    return SALTS_CRYPTO_OK;
}
static int sha512_add(salts_crypto_sha512_ctx_t *ctx, const void *data,
                      size_t size, uint32_t variant) {
    salts_crypto_sha512_impl local;
    const uint8_t *bytes=(const uint8_t *)data;
    size_t remaining=size;
    uint64_t low,high,increment;
    if (!ctx || (!data && size)) return SALTS_CRYPTO_EINVAL;
    memcpy(&local,ctx->bytes,sizeof(local));
    if (local.magic!=variant || local.used>=SALTS_SHA512_BLOCK_BYTES) {
        salts_crypto_secure_zero(&local,sizeof(local));
        return SALTS_CRYPTO_ESTATE;
    }
    increment=(uint64_t)size;
    if ((size_t)increment!=size) {
        salts_crypto_secure_zero(&local,sizeof(local));
        return SALTS_CRYPTO_EINVAL;
    }
    low=local.length_low+increment;
    high=local.length_high+(low<local.length_low ? 1u : 0u);
    /* FIPS 180-4 128-bit bit count: detect overflow BEFORE modification. */
    if (high>UINT64_MAX/8u || high<local.length_high) {
        salts_crypto_secure_zero(&local,sizeof(local));
        return SALTS_CRYPTO_EINVAL;
    }
    while (remaining) {
        size_t take=SALTS_SHA512_BLOCK_BYTES-local.used;
        if (take>remaining) take=remaining;
        memcpy(local.block+local.used,bytes,take);
        bytes+=take; remaining-=take; local.used+=(uint32_t)take;
        if (local.used==SALTS_SHA512_BLOCK_BYTES) {
            sha512_compress(&local,local.block);
            local.used=0u;
        }
    }
    local.length_low=low; local.length_high=high;
    memcpy(ctx->bytes,&local,sizeof(local));
    salts_crypto_secure_zero(&local,sizeof(local));
    return SALTS_CRYPTO_OK;
}
static int sha512_finish(salts_crypto_sha512_ctx_t *ctx, uint8_t *digest,
                         uint32_t variant, size_t digest_size) {
    salts_crypto_sha512_impl local;
    uint64_t high_bits,low_bits;
    if (!ctx || !digest) return SALTS_CRYPTO_EINVAL;
    memcpy(&local,ctx->bytes,sizeof(local));
    if (local.magic!=variant || local.used>=SALTS_SHA512_BLOCK_BYTES) {
        salts_crypto_secure_zero(&local,sizeof(local));
        return SALTS_CRYPTO_ESTATE;
    }
    high_bits=(local.length_high<<3u)|(local.length_low>>61u);
    low_bits=local.length_low<<3u;
    local.block[local.used++]=0x80u;
    if (local.used>112u) {
        memset(local.block+local.used,0,SALTS_SHA512_BLOCK_BYTES-local.used);
        sha512_compress(&local,local.block);
        local.used=0u;
    }
    memset(local.block+local.used,0,112u-local.used);
    sha512_store64(local.block+112u,high_bits);
    sha512_store64(local.block+120u,low_bits);
    sha512_compress(&local,local.block);
    for (size_t i=0u;i<digest_size/8u;++i)
        sha512_store64(digest+8u*i,local.state[i]);
    salts_crypto_secure_zero(ctx,sizeof(*ctx));
    salts_crypto_secure_zero(&local,sizeof(local));
    return SALTS_CRYPTO_OK;
}
int salts_crypto_sha384_init(salts_crypto_sha384_ctx_t *ctx) {
    return sha512_start(ctx,SALTS_SHA384_MAGIC);
}
int salts_crypto_sha384_update(salts_crypto_sha384_ctx_t *ctx,
                               const void *data,size_t size) {
    return sha512_add(ctx,data,size,SALTS_SHA384_MAGIC);
}
int salts_crypto_sha384_final(salts_crypto_sha384_ctx_t *ctx,
                              uint8_t digest[SALTS_CRYPTO_SHA384_DIGEST_SIZE]) {
    return sha512_finish(ctx,digest,SALTS_SHA384_MAGIC,SALTS_CRYPTO_SHA384_DIGEST_SIZE);
}
int salts_crypto_sha512_init(salts_crypto_sha512_ctx_t *ctx) {
    return sha512_start(ctx,SALTS_SHA512_MAGIC);
}
int salts_crypto_sha512_update(salts_crypto_sha512_ctx_t *ctx,
                               const void *data,size_t size) {
    return sha512_add(ctx,data,size,SALTS_SHA512_MAGIC);
}
int salts_crypto_sha512_final(salts_crypto_sha512_ctx_t *ctx,
                              uint8_t digest[SALTS_CRYPTO_SHA512_DIGEST_SIZE]) {
    return sha512_finish(ctx,digest,SALTS_SHA512_MAGIC,SALTS_CRYPTO_SHA512_DIGEST_SIZE);
}
int salts_crypto_sha384(const void *data,size_t size,
                        uint8_t digest[SALTS_CRYPTO_SHA384_DIGEST_SIZE]) {
    salts_crypto_sha384_ctx_t ctx;
    int status;
    if (!digest || (!data && size)) return SALTS_CRYPTO_EINVAL;
    status=salts_crypto_sha384_init(&ctx);
    if (status==SALTS_CRYPTO_OK) status=salts_crypto_sha384_update(&ctx,data,size);
    if (status==SALTS_CRYPTO_OK) status=salts_crypto_sha384_final(&ctx,digest);
    salts_crypto_secure_zero(&ctx,sizeof(ctx));
    if (status!=SALTS_CRYPTO_OK)
        salts_crypto_secure_zero(digest,SALTS_CRYPTO_SHA384_DIGEST_SIZE);
    return status;
}
int salts_crypto_sha512(const void *data,size_t size,
                        uint8_t digest[SALTS_CRYPTO_SHA512_DIGEST_SIZE]) {
    salts_crypto_sha512_ctx_t ctx;
    int status;
    if (!digest || (!data && size)) return SALTS_CRYPTO_EINVAL;
    status=salts_crypto_sha512_init(&ctx);
    if (status==SALTS_CRYPTO_OK) status=salts_crypto_sha512_update(&ctx,data,size);
    if (status==SALTS_CRYPTO_OK) status=salts_crypto_sha512_final(&ctx,digest);
    salts_crypto_secure_zero(&ctx,sizeof(ctx));
    if (status!=SALTS_CRYPTO_OK)
        salts_crypto_secure_zero(digest,SALTS_CRYPTO_SHA512_DIGEST_SIZE);
    return status;
}
