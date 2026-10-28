/* SPDX-License-Identifier: Apache-2.0
 * Versioned first-party C11 TLS policy owner, within ONE Salts::Crypto.
 * NOT a second TLS engine, not a session constructor, not proof that
 * the incumbent TLS backend supports any particular verified profile.
 */
#ifndef CRYPTO_TLS_PROFILE_H_INCLUDED
#define CRYPTO_TLS_PROFILE_H_INCLUDED

#include <crypto/tls.h>
#include <crypto/tls_auth.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTO_TLS_PROFILE_ABI_VERSION UINT32_C(0x00010004)
#define CRYPTO_TLS_PROFILE_PATH_MAX_BYTES 4095u
#define CRYPTO_TLS_PROFILE_PASSWORD_MAX_BYTES 1023u
#define CRYPTO_TLS_PROFILE_NAME_MAX_BYTES 255u
#define CRYPTO_TLS_PROFILE_ALPN_MAX_WIRE_BYTES 65535u
#define CRYPTO_TLS_PROFILE_TLCP11 UINT32_C(0x0101)

typedef struct crypto_tls_profile crypto_tls_profile;

typedef enum crypto_tls_profile_result {
    CRYPTO_TLS_PROFILE_OK = 0,
    CRYPTO_TLS_PROFILE_INVALID_ARGUMENT = -1,
    CRYPTO_TLS_PROFILE_RANGE = -2,
    CRYPTO_TLS_PROFILE_OUT_OF_MEMORY = -3,
    CRYPTO_TLS_PROFILE_BUFFER_TOO_SMALL = -4,
    CRYPTO_TLS_PROFILE_ABSENT = -5
} crypto_tls_profile_result;

typedef enum crypto_tls_security_profile_v1_4 {
    CRYPTO_TLS_PROFILE_STANDARD = 0,
    CRYPTO_TLS_PROFILE_SHANGMI_TLS13 = 1,
    CRYPTO_TLS_PROFILE_TLCP11_MODE = 2
} crypto_tls_security_profile_v1_4;

typedef enum crypto_tls_trust_mode_v1_4 {
    CRYPTO_TLS_TRUST_NONE = 0,
    CRYPTO_TLS_TRUST_EXPLICIT = 1,
    CRYPTO_TLS_TRUST_SYSTEM = 2
} crypto_tls_trust_mode_v1_4;

/* RFC 7301 ALPN wire labels, NOT C strings. Embedded NUL is valid.
 * Each label is 1..255 bytes; 1-byte length prefixes make a combined
 * bounded wire payload <=65535. Labels are borrowed for create only. */
typedef struct crypto_tls_alpn_label_v1_4 {
    const uint8_t *data;
    size_t size;
} crypto_tls_alpn_label_v1_4;

/* This is a policy structure, not capability negotiation and NOT a
 * session-create API. Complete CNet TLS1.2/1.3, ShangMi TLS1.3, TLCP1.1,
 * CA file/hashed directory/system trust, DNS/IP verification, independent
 * SNI and identity, and binary ALPN all need real engine/runtime proofs
 * before production cutover. No unsupported field is silently discarded.
 *
 * CLIENT: verified_name is required; sni_name optional and independent.
 * Trust must be SYSTEM (no CA paths) or EXPLICIT (file/path present).
 * Client identity cert/key may be both NULL or both present. TLCP client
 * certificates are intentionally rejected (original CNet policy).
 *
 * SERVER: identity cert/key are required. verified_name and sni_name must
 * be NULL. Client-auth NONE requires trust NONE and no CA inputs; REQUIRED
 * requires EXPLICIT file/path trust. TLCP REQUIRED is not supported by
 * the original public CNet behavior and is rejected here.
 *
 * STANDARD: protocol_min/max in [TLS1.2,TLS1.3], min<=max.
 * SHANGMI_TLS13: min=max TLS1.3. TLCP11: min=max TLCP1.1.
 * verify_depth=0 selects the existing default, otherwise 1..5.
 * All path strings are bounded, caller-borrowed only until create returns.
 * The owner deep-copies every accepted path/password/hostname/ALPN label.
 */
typedef struct crypto_tls_profile_options_v1_4 {
    uint32_t struct_size;
    uint32_t role;                 /* crypto_tls_role */
    uint32_t security_profile;     /* crypto_tls_security_profile_v1_4 */
    uint32_t trust_mode;           /* crypto_tls_trust_mode_v1_4 */
    uint32_t protocol_min;
    uint32_t protocol_max;
    uint32_t verify_depth;
    uint32_t client_auth;          /* CRYPTO_TLS_CLIENT_AUTH_* */
    uint32_t reserved_flags;       /* must equal zero */
    uint32_t reserved;             /* must equal zero */
    const char *verified_name;     /* client-only DNS/IP identity */
    const char *sni_name;          /* independent client SNI name */
    const char *ca_file;
    const char *ca_path;
    const char *certificate_file;  /* TLCP server: sign+encrypt chain */
    const char *private_key_file;  /* TLCP server: paired private keys */
    const char *private_key_password;
    const crypto_tls_alpn_label_v1_4 *alpn;
    size_t alpn_count;
} crypto_tls_profile_options_v1_4;

typedef struct crypto_tls_profile_info_v1_4 {
    uint32_t struct_size; /* caller sets sizeof; unchanged on errors */
    uint32_t role;
    uint32_t security_profile;
    uint32_t trust_mode;
    uint32_t protocol_min;
    uint32_t protocol_max;
    uint32_t verify_depth;
    uint32_t client_auth;
    size_t alpn_count;
    size_t alpn_wire_bytes;
} crypto_tls_profile_info_v1_4;

#define CRYPTO_TLS_PROFILE_NAME_VERIFIED 1u
#define CRYPTO_TLS_PROFILE_NAME_SNI 2u

CRYPTO_TLS_API uint32_t crypto_tls_profile_abi_version(void);

/* On failure *out_profile is UNCHANGED; no backend/session is created. */
CRYPTO_TLS_API crypto_tls_profile_result crypto_tls_profile_create_v1_4(
    const crypto_tls_profile_options_v1_4 *options,
    crypto_tls_profile **out_profile);

/* On error *out_info is unchanged, including struct_size. */
CRYPTO_TLS_API crypto_tls_profile_result crypto_tls_profile_get_info_v1_4(
    const crypto_tls_profile *profile, crypto_tls_profile_info_v1_4 *out_info);

/* Copies owned ALPN length-prefixed wire bytes to caller storage.
 * For too-small buffers *out_size reports required size; output unchanged.
 * A size-zero list permits out_data=NULL and capacity=0. */
CRYPTO_TLS_API crypto_tls_profile_result crypto_tls_profile_copy_alpn_wire_v1_4(
    const crypto_tls_profile *profile, uint8_t *out_data,
    size_t capacity, size_t *out_size);

/* Copies NUL-terminated name; *out_size is total bytes INCLUDING NUL.
 * Absent names yield ABSENT leaving output/out_size unchanged.
 * For too-small buffers, *out_size reports required size; output unchanged. */
CRYPTO_TLS_API crypto_tls_profile_result crypto_tls_profile_copy_name_v1_4(
    const crypto_tls_profile *profile, uint32_t name_kind,
    char *out_data, size_t capacity, size_t *out_size);

/* Quiescent owner retirement. All borrowed input has already been cloned.
 * The entire allocation, including any stored password, is securely wiped. */
CRYPTO_TLS_API crypto_tls_profile_result crypto_tls_profile_destroy_v1_4(
    crypto_tls_profile **profile);

/*
 * Stage-1 additive REAL session creation using the EXISTING sole
 * Salts::Crypto/GmSSL-backed TLS implementation. Explicitly return
 * UNSUPPORTED for profiles it cannot honestly implement; NEVER switch
 * engines, change protocol/version, weaken trust or silently omit ALPN.
 *
 * Currently admitted subset: STANDARD exact TLS1.2 or TLS1.3 only, DNS
 * identity (not IP SAN) with equal/default SNI, explicit CA FILE only,
 * RSA original suite, optional client identity / REQUIRED server mTLS,
 * and <=8 printable-ASCII ALPN labels <=127 bytes / 128 wire bytes.
 * System trust, CA path, independent SNI, version range, binary ALPN,
 * TLCP/ShangMi and other unqualified suites are UNSUPPORTED, not emulated.
 *
 * out_session must point to NULL; remains NULL on every error. The opaque
 * Crypto Session owns its backend association, never the profile or IO
 * callback user. Profile may be retired after this create returns because
 * the backend constructor consumes the copied policy synchronously.
 * This is NOT production CNet full-profile admission.
 */
#define CRYPTO_TLS_PROFILE_UNSUPPORTED ((crypto_tls_result)-10)
CRYPTO_TLS_API crypto_tls_result crypto_tls_session_create_profile_v1_4(
    crypto_tls_provider *provider, const crypto_tls_profile *profile,
    const crypto_tls_io *io, crypto_tls_session **out_session);


#ifdef __cplusplus
}
#endif
#endif /* CRYPTO_TLS_PROFILE_H_INCLUDED */
