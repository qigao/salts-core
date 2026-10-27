/* SPDX-License-Identifier: Apache-2.0
 * Installed static Salts::Crypto / real CNet C++17 header and runtime probe.
 */
#include <cnet/cnet.h>
#include <crypto/tls.h>
#include <crypto/tls_ext.h>
#include <crypto/tls_peer.h>
#include <crypto/cipher_bio.h>
#include <salts_core/runtime_abi.h>

#include <type_traits>

static_assert(std::is_standard_layout<cnet_tls_client>::value, "CNet client ABI");
static_assert(std::is_standard_layout<cnet_tls_server>::value, "CNet server ABI");

int main()
{
    salts_core_runtime_abi_0_1_required();
    if (salts_core_runtime_abi_version() != SALTS_CORE_RUNTIME_ABI_VERSION ||
        crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION ||
        crypto_tls_extension_abi_version() != CRYPTO_TLS_EXTENSION_ABI_VERSION ||
        crypto_tls_peer_abi_version() != CRYPTO_TLS_PEER_ABI_VERSION)
        return 1;
    crypto_tls_provider *provider = nullptr;
    if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider) != CRYPTO_TLS_OK ||
        provider == nullptr)
        return 2;
    if (crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK || provider != nullptr)
        return 3;
    return 0;
}
