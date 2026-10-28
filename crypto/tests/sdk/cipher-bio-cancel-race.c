/* SPDX-License-Identifier: Apache-2.0
 * Native Linux source-TSan borrowed ciphertext callback vs concurrent cancel.
 * The TLS/NativeIO owner always joins before releasing its borrowed descriptor.
 */
#include <crypto/cipher_bio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>

enum { CYCLES = 64, MAX_SPINS = 3000000 };

typedef struct callback_worker {
    crypto_tls_io io;
    atomic_uint iterations;
    int outcome;
} callback_worker;

static void *drive_reads(void *data)
{
    callback_worker *w = (callback_worker *)data;
    uint8_t one[7] = {0};
    unsigned i;
    for (i = 0u; i < MAX_SPINS; ++i) {
        int64_t ret = w->io.read(w->io.user, one, sizeof(one));
        if (ret == CRYPTO_TLS_IO_ERROR) {
            w->outcome = 0;
            return NULL;
        }
        if (ret != CRYPTO_TLS_IO_AGAIN) {
            w->outcome = 2;
            return NULL;
        }
        atomic_fetch_add_explicit(&w->iterations, 1u,
                                  memory_order_release);
    }
    w->outcome = 3;
    return NULL;
}

static int cycle(int index)
{
    crypto_tls_cipher_bio *bio = NULL;
    crypto_tls_cipher_bio_snapshot snapshot = {0};
    callback_worker worker;
    pthread_t thread_id;
    unsigned spins;
    int started = 0, okay = 0;

    worker.io = (crypto_tls_io){0};
    atomic_init(&worker.iterations, 0u);
    worker.outcome = 1;
    if (crypto_tls_cipher_bio_create(
            CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY, &bio) != CRYPTO_TLS_OK
        || crypto_tls_cipher_bio_acquire_io(bio, &worker.io)
           != CRYPTO_TLS_OK)
        goto done;
    if (pthread_create(&thread_id, NULL, drive_reads, &worker) != 0)
        goto done;
    started = 1;
    for (spins = 0u; spins < MAX_SPINS; ++spins) {
        if (atomic_load_explicit(&worker.iterations,
                memory_order_acquire) >= 1000u)
            break;
        sched_yield();
    }
    if (spins == MAX_SPINS) goto done;

    /* Cancel from an independent thread while the worker is repeatedly
     * entering the actual borrowed IO read callback. No BIO state freed. */
    if (crypto_tls_cipher_bio_cancel(bio) != CRYPTO_TLS_OK
        || crypto_tls_cipher_bio_destroy(&bio) != CRYPTO_TLS_BUSY
        || !bio)
        goto done;
    okay = 1;
done:
    if (started) {
        /* Idempotent cancel ensures a failing preliminary assertion cannot
         * deadlock the borrowed IO worker or race the final BIO release. */
        if (bio) (void)crypto_tls_cipher_bio_cancel(bio);
        pthread_join(thread_id, NULL);
        if (worker.outcome != 0) okay = 0;
    } else {
        okay = 0;
    }
    if (bio) {
        snapshot.struct_size = sizeof(snapshot);
        if (crypto_tls_cipher_bio_get_snapshot(bio, &snapshot)
            != CRYPTO_TLS_OK || snapshot.input_pending != 0u)
            okay = 0;
        if (crypto_tls_cipher_bio_release_io(bio) != CRYPTO_TLS_OK
            || crypto_tls_cipher_bio_destroy(&bio) != CRYPTO_TLS_OK || bio)
            okay = 0;
    }
    if (!okay) fprintf(stderr, "Crypto CipherBIO cancel race %d failed\n", index);
    return okay ? 0 : 1;
}

int main(void)
{
    int i;
    for (i = 0; i < CYCLES; ++i)
        if (cycle(i) != 0) return 1;
    puts("Crypto bounded CipherBIO 64 real concurrent cancel/IO lease cycles: PASS");
    return 0;
}
