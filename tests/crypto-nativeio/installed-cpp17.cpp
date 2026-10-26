/* SPDX-License-Identifier: Apache-2.0
 * Independent installed C++17 ABI/link graph smoke. The separate C11
 * real_tcp_tls test is the REQUIRED genuine overlapped IOCP data-path gate.
 */
#include <salts/native_io.h>
#include <salts_core/runtime_abi.h>
#include <crypto/tls.h>
#include <crypto/cipher_bio.h>

int main()
{
    salts_core_runtime_abi_0_1_required();
    if (salts_core_runtime_abi_version() != SALTS_CORE_RUNTIME_ABI_VERSION
        || crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION
        || native_io_backend_kind_model(NATIVE_IO_BACKEND_IOCP)
              != NATIVE_IO_MODEL_COMPLETION
        || !native_io_backend_kind_supported(NATIVE_IO_BACKEND_IOCP))
        return 1;

    native_io_backend backend{};
    const native_io_backend_config config{NATIVE_IO_BACKEND_IOCP, 1u, 2u, 2u};
    if (native_io_backend_init(&backend, &config) != SALTS_OK)
        return 2;
    if (native_io_backend_close(&backend) != SALTS_OK
        || native_io_backend_destroy(&backend) != SALTS_OK)
        return 3;

    crypto_tls_provider *provider = nullptr;
    if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider)
        != CRYPTO_TLS_OK)
        return 4;
    crypto_tls_cipher_bio *bio = nullptr;
    if (crypto_tls_cipher_bio_create(
            CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY, &bio) != CRYPTO_TLS_OK)
        return 5;
    if (crypto_tls_cipher_bio_destroy(&bio) != CRYPTO_TLS_OK
        || crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK)
        return 6;
    return 0;
}
