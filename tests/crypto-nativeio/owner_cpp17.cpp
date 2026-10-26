/* SPDX-License-Identifier: Apache-2.0
 * Independent installed C++17 two-runtime ABI owner graph.
 * NO private GmSSL symbols, NO old Salts TLS backend and NO second DLL.
 */
#include <salts/native_io.h>
#include <salts/error_codes.h>
#include <cmeta_buffer.h>
#include <crypto/provider.h>
#include <crypto/tls.h>
#include <crypto/tls_ext.h>
#include <crypto/tls_peer.h>
#include <crypto/cipher_bio.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

static native_io_backend_kind selected_backend() noexcept
{
#if defined(_WIN32)
    return NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
    return NATIVE_IO_BACKEND_KQUEUE;
#elif defined(__linux__)
    return NATIVE_IO_BACKEND_EPOLL;
#else
#error Only real Windows IOCP, macOS kqueue and Linux epoll qualify
#endif
}

int main()
{
    static constexpr std::array<std::uint8_t, 32> known_sm3 = {
        0x66,0xc7,0xf0,0xf4,0x62,0xee,0xed,0xd9,
        0xd1,0xf2,0xd4,0x6b,0xdc,0x10,0xe4,0xe2,
        0x41,0x67,0xc4,0x48,0x75,0xcf,0x2f,0x7a,
        0x22,0x97,0xda,0x02,0xb8,0xf4,0xba,0x8e
    };
    std::array<std::uint8_t, 32> digest = {};
    static constexpr std::uint8_t input[] = {'a','b','c'};
    crypto_tls_provider *provider = nullptr;
    crypto_tls_cipher_bio *bio = nullptr;
    crypto_tls_io io = {};
    native_io_backend backend = {};
    mem_buffer_t *buffer = nullptr;
    crypto_tls_cipher_bio_snapshot snapshot = {};
    native_io_backend_config cfg = {};
    native_io_backend_stats stats = {};
    bool backend_live = false, owner_ok = false;
    int rc = 1;

    if (crypto_provider_runtime_abi() != CRYPTO_PROVIDER_ABI_VERSION ||
        crypto_provider_sm3(input, sizeof(input), digest.data(), digest.size())
          != CRYPTO_PROVIDER_OK ||
        std::memcmp(digest.data(), known_sm3.data(), digest.size()) != 0)
        goto done;

    if (crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION ||
        crypto_tls_extension_abi_version() != CRYPTO_TLS_EXTENSION_ABI_VERSION ||
        crypto_tls_peer_abi_version() != CRYPTO_TLS_PEER_ABI_VERSION)
        goto done;
    if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider)
          != CRYPTO_TLS_OK || !provider)
        goto done;

    if (crypto_tls_cipher_bio_create(
          CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY, &bio) != CRYPTO_TLS_OK)
        goto done;
    if (crypto_tls_cipher_bio_acquire_io(bio, &io) != CRYPTO_TLS_OK ||
        io.user != bio || !io.write || !io.read ||
        crypto_tls_cipher_bio_destroy(&bio) != CRYPTO_TLS_BUSY || !bio)
        goto done;
    snapshot.struct_size = sizeof(snapshot);
    if (crypto_tls_cipher_bio_get_snapshot(bio, &snapshot) != CRYPTO_TLS_OK ||
        snapshot.capacity != CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY ||
        snapshot.input_pending != 0u || snapshot.output_pending != 0u)
        goto done;

    cfg = {selected_backend(), 2u, 4u, 2u};
    if (!native_io_backend_kind_supported(cfg.kind) ||
        native_io_backend_kind_model(cfg.kind) == NATIVE_IO_MODEL_NONE ||
        native_io_backend_init(&backend, &cfg) != SALTS_OK)
        goto done;
    backend_live = true;
    if (!native_io_backend_get_stats(&backend, &stats) ||
        stats.active_requests != 0u ||
        stats.endpoint_count != 0u)
        goto done;

    /* This buffer is owned by the single installed Core runtime, not an
     * inline copy or another legacy static core allocator.
     */
    buffer = mem_get_buffer(mem_global(), 256u);
    if (!buffer || mem_buffer_capacity(buffer) < 256u)
        goto done;

    owner_ok = true;
    rc = 0;
done:
    if (buffer) mem_buffer_release(buffer);
    if (backend_live) {
        if (native_io_backend_close(&backend) != SALTS_OK ||
            native_io_backend_destroy(&backend) != SALTS_OK)
            rc = 1;
    }
    if (bio) {
        if (crypto_tls_cipher_bio_release_io(bio) != CRYPTO_TLS_OK ||
            crypto_tls_cipher_bio_destroy(&bio) != CRYPTO_TLS_OK || bio)
            rc = 1;
    }
    if (provider &&
        (crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK || provider))
        rc = 1;
    if (!owner_ok || rc) {
        std::fputs("Crypto+SaltsCore installed C++17 one-owner ABI graph failed\n",
                   stderr);
        return 1;
    }
    std::puts("Crypto+SaltsCore installed C++17 SM3/TLS/BIO/NativeIO shared owner PASS");
    return 0;
}
