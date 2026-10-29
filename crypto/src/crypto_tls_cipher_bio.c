/* SPDX-License-Identifier: Apache-2.0
 * Provider-neutral fixed ciphertext rings. NO GmSSL linkage, sockets,
 * plaintext buffering, thread pool, persistence or TLS state.
 */
#include <crypto/cipher_bio.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct crypto_tls_cipher_bio {
    size_t capacity;
    size_t input_head;
    size_t input_size;
    size_t output_head;
    size_t output_size;
    unsigned char *input;
    unsigned char *output;
    atomic_bool io_leased;
    atomic_bool cancelled;
    atomic_bool input_eof;
    atomic_uint active_callbacks;
    unsigned char storage[];
};

static size_t min_size(size_t left, size_t right)
{
    return left < right ? left : right;
}

/* Precondition: 0 <= head,used <= size, head < size, count <= size-used. */
static void ring_write(unsigned char *ring, size_t size, size_t head,
                       size_t used, const uint8_t *src, size_t count)
{
    const size_t offset = (head + used) % size;
    const size_t first = min_size(count, size - offset);
    memcpy(ring + offset, src, first);
    if (count > first) memcpy(ring, src + first, count - first);
}

/* Precondition: count <= used, head < size. */
static void ring_read(unsigned char *ring, size_t size,
                      size_t *head, size_t *used,
                      uint8_t *dest, size_t count)
{
    const size_t first = min_size(count, size - *head);
    memcpy(dest, ring + *head, first);
    if (count > first) memcpy(dest + first, ring, count - first);
    /* Erase only consumed bytes, never not-yet-consumed ring bytes. */
    memset(ring + *head, 0, first);
    if (count > first) memset(ring, 0, count - first);
    *head = (*head + count) % size;
    *used -= count;
}

static int64_t tls_cipher_write(void *user, const uint8_t *data, size_t len)
{
    crypto_tls_cipher_bio *bio = (crypto_tls_cipher_bio *)user;
    size_t space, take;
    if (!bio) return CRYPTO_TLS_IO_ERROR;
    atomic_fetch_add_explicit(&bio->active_callbacks, 1u,
                              memory_order_acq_rel);
    if (!data || !len ||
        atomic_load_explicit(&bio->cancelled, memory_order_acquire)) {
        atomic_fetch_sub_explicit(&bio->active_callbacks, 1u,
                                  memory_order_acq_rel);
        return CRYPTO_TLS_IO_ERROR;
    }
    space = bio->capacity - bio->output_size;
    take = min_size(space, len);
    if (take) {
        ring_write(bio->output, bio->capacity, bio->output_head,
                   bio->output_size, data, take);
        bio->output_size += take;
    }
    atomic_fetch_sub_explicit(&bio->active_callbacks, 1u,
                              memory_order_acq_rel);
    return take ? (int64_t)take : CRYPTO_TLS_IO_AGAIN;
}

static int64_t tls_cipher_read(void *user, uint8_t *out, size_t len)
{
    crypto_tls_cipher_bio *bio = (crypto_tls_cipher_bio *)user;
    int64_t result;
    size_t take;
    if (!bio) return CRYPTO_TLS_IO_ERROR;
    atomic_fetch_add_explicit(&bio->active_callbacks, 1u,
                              memory_order_acq_rel);
    if (!out || !len ||
        atomic_load_explicit(&bio->cancelled, memory_order_acquire)) {
        result = CRYPTO_TLS_IO_ERROR;
    } else if (bio->input_size) {
        take = min_size(len, bio->input_size);
        ring_read(bio->input, bio->capacity, &bio->input_head,
                  &bio->input_size, out, take);
        result = (int64_t)take;
    } else if (atomic_load_explicit(&bio->input_eof, memory_order_acquire)) {
        /* A transport EOF is NOT a validated TLS close_notify. */
        result = 0;
    } else {
        result = CRYPTO_TLS_IO_AGAIN;
    }
    atomic_fetch_sub_explicit(&bio->active_callbacks, 1u,
                              memory_order_acq_rel);
    return result;
}

crypto_tls_result crypto_tls_cipher_bio_create(
    size_t capacity, crypto_tls_cipher_bio **out_bio)
{
    crypto_tls_cipher_bio *bio;
    if (!out_bio) return CRYPTO_TLS_INVALID_ARGUMENT;
    *out_bio = NULL;
    if (capacity < CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY
        || capacity > CRYPTO_TLS_CIPHER_BIO_MAX_CAPACITY
        || capacity > (SIZE_MAX - sizeof(*bio)) / 2u)
        return CRYPTO_TLS_INVALID_ARGUMENT;

    bio = (crypto_tls_cipher_bio *)calloc(
        1u, sizeof(*bio) + capacity * 2u);
    if (!bio) return CRYPTO_TLS_OUT_OF_MEMORY;
    bio->capacity = capacity;
    bio->input = bio->storage;
    bio->output = bio->storage + capacity;
    atomic_init(&bio->io_leased, false);
    atomic_init(&bio->cancelled, false);
    atomic_init(&bio->input_eof, false);
    atomic_init(&bio->active_callbacks, 0u);
    *out_bio = bio;
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_acquire_io(
    crypto_tls_cipher_bio *bio, crypto_tls_io *out_io)
{
    bool expected = false;
    crypto_tls_io result;
    if (!bio || !out_io) return CRYPTO_TLS_INVALID_ARGUMENT;
    if (atomic_load_explicit(&bio->cancelled, memory_order_acquire))
        return CRYPTO_TLS_CANCELLED;
    if (!atomic_compare_exchange_strong_explicit(
            &bio->io_leased, &expected, true,
            memory_order_acq_rel, memory_order_acquire))
        return CRYPTO_TLS_BUSY;
    memset(&result, 0, sizeof(result));
    result.struct_size = (uint32_t)sizeof(result);
    result.user = bio;
    result.write = tls_cipher_write;
    result.read = tls_cipher_read;
    *out_io = result;
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_release_io(
    crypto_tls_cipher_bio *bio)
{
    if (!bio) return CRYPTO_TLS_INVALID_ARGUMENT;
    if (!atomic_load_explicit(&bio->io_leased, memory_order_acquire))
        return CRYPTO_TLS_INVALID_ARGUMENT;
    if (atomic_load_explicit(&bio->active_callbacks, memory_order_acquire))
        return CRYPTO_TLS_BUSY;
    /* Host has already destroyed the Crypto TLS session and joined all
     * potential callbacks. This does NOT safely detach a live session. */
    atomic_store_explicit(&bio->io_leased, false, memory_order_release);
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_feed_cipher(
    crypto_tls_cipher_bio *bio, const uint8_t *cipher, size_t size)
{
    if (!bio || !cipher || !size) return CRYPTO_TLS_INVALID_ARGUMENT;
    if (atomic_load_explicit(&bio->cancelled, memory_order_acquire))
        return CRYPTO_TLS_CANCELLED;
    if (atomic_load_explicit(&bio->input_eof, memory_order_acquire))
        return CRYPTO_TLS_CLOSED;
    /* All-or-nothing, including wraparound, same as CNet admission. */
    if (size > bio->capacity - bio->input_size)
        return CRYPTO_TLS_BIO_CAPACITY;
    ring_write(bio->input, bio->capacity, bio->input_head,
               bio->input_size, cipher, size);
    bio->input_size += size;
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_take_cipher(
    crypto_tls_cipher_bio *bio, uint8_t *cipher, size_t capacity,
    size_t *out_size)
{
    size_t taken;
    if (!out_size) return CRYPTO_TLS_INVALID_ARGUMENT;
    *out_size = 0u;
    if (!bio || !cipher || !capacity) return CRYPTO_TLS_INVALID_ARGUMENT;
    if (!bio->output_size) return CRYPTO_TLS_AGAIN;
    taken = min_size(capacity, bio->output_size);
    ring_read(bio->output, bio->capacity, &bio->output_head,
              &bio->output_size, cipher, taken);
    *out_size = taken;
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_end_input(
    crypto_tls_cipher_bio *bio)
{
    if (!bio) return CRYPTO_TLS_INVALID_ARGUMENT;
    if (atomic_load_explicit(&bio->cancelled, memory_order_acquire))
        return CRYPTO_TLS_CANCELLED;
    atomic_store_explicit(&bio->input_eof, true, memory_order_release);
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_cancel(
    crypto_tls_cipher_bio *bio)
{
    if (!bio) return CRYPTO_TLS_INVALID_ARGUMENT;
    atomic_store_explicit(&bio->cancelled, true, memory_order_release);
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_get_snapshot(
    const crypto_tls_cipher_bio *bio,
    crypto_tls_cipher_bio_snapshot *out_snapshot)
{
    crypto_tls_cipher_bio_snapshot snapshot;
    if (!bio || !out_snapshot ||
        out_snapshot->struct_size != sizeof(*out_snapshot))
        return CRYPTO_TLS_INVALID_ARGUMENT;
    snapshot.struct_size = (uint32_t)sizeof(snapshot);
    snapshot.input_eof =
        atomic_load_explicit(&bio->input_eof, memory_order_acquire) ? 1u : 0u;
    snapshot.capacity = bio->capacity;
    snapshot.input_pending = bio->input_size;
    snapshot.output_pending = bio->output_size;
    *out_snapshot = snapshot;
    return CRYPTO_TLS_OK;
}

crypto_tls_result crypto_tls_cipher_bio_destroy(
    crypto_tls_cipher_bio **out_bio)
{
    crypto_tls_cipher_bio *bio;
    volatile unsigned char *wipe;
    size_t i;
    if (!out_bio || !*out_bio) return CRYPTO_TLS_INVALID_ARGUMENT;
    bio = *out_bio;
    if (atomic_load_explicit(&bio->io_leased, memory_order_acquire)
        || atomic_load_explicit(&bio->active_callbacks, memory_order_acquire))
        return CRYPTO_TLS_BUSY;
    *out_bio = NULL;
    wipe = bio->storage;
    for (i = 0u; i < bio->capacity * 2u; ++i) wipe[i] = 0u;
    free(bio);
    return CRYPTO_TLS_OK;
}
