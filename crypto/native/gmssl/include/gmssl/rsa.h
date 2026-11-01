/*
 *  Copyright 2014-2022 The GmSSL Project. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the License); you may
 *  not use this file except in compliance with the License.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 */



#ifndef GMSSL_RSA_H
#define GMSSL_RSA_H


#include <stdio.h>
#include <string.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
RSAPublicKey ::= SEQUENCE {
	modulus			INTEGER,  -- n
	publicExponent		INTEGER   -- e
}

RSAPrivateKey ::= SEQUENCE {
	version			INTEGER, -- 0
	modulus			INTEGER, -- n
	publicExponent		INTEGER, -- e
	privateExponent		INTEGER, -- d
	prime1			INTEGER, -- p
	prime2			INTEGER, -- q
	exponent1		INTEGER, -- d mod (p-1)
	exponent2		INTEGER, -- d mod (q-1)
	coefficient		INTEGER  -- q^-1 mod p
}
*/


#define RSA_MIN_MODULUS_SIZE 256
#define RSA_MAX_MODULUS_SIZE 512
#define RSA_MAX_MODULUS_WORDS (RSA_MAX_MODULUS_SIZE / 4)

typedef struct {
	size_t modulus_size;
	uint8_t modulus[RSA_MAX_MODULUS_SIZE];
	uint32_t public_exponent;
} RSA_PUBLIC_KEY;

#define RSA_MAX_PRIME_SIZE (RSA_MAX_MODULUS_SIZE / 2)

typedef struct {
	RSA_PUBLIC_KEY public_key;
	size_t prime_size;
	uint8_t prime1[RSA_MAX_PRIME_SIZE];
	uint8_t prime2[RSA_MAX_PRIME_SIZE];
	uint8_t exponent1[RSA_MAX_PRIME_SIZE];
	uint8_t exponent2[RSA_MAX_PRIME_SIZE];
	uint8_t coefficient[RSA_MAX_PRIME_SIZE];
} RSA_PRIVATE_KEY;

int rsa_private_key_from_der(RSA_PRIVATE_KEY *key, const uint8_t **in, size_t *inlen);
int rsa_private_key_operation(const RSA_PRIVATE_KEY *key,
	const uint8_t *in, size_t inlen,
	uint8_t *out, size_t outmax, size_t *outlen);
int rsa_sign_pkcs1_v15_sha256(const RSA_PRIVATE_KEY *key,
	const uint8_t dgst[32], uint8_t *sig, size_t sigmax, size_t *siglen);
int rsa_sign_pss_sha256_with_salt(const RSA_PRIVATE_KEY *key,
	const uint8_t dgst[32], const uint8_t salt[32],
	uint8_t *sig, size_t sigmax, size_t *siglen);
int rsa_sign_pss_sha256(const RSA_PRIVATE_KEY *key,
	const uint8_t dgst[32], uint8_t *sig, size_t sigmax, size_t *siglen);
int rsa_private_key_info_from_der(RSA_PRIVATE_KEY *key,
	const uint8_t **attrs, size_t *attrslen,
	const uint8_t **in, size_t *inlen);
int rsa_private_key_info_from_pem(RSA_PRIVATE_KEY *key, FILE *fp);
void rsa_private_key_cleanup(RSA_PRIVATE_KEY *key);
int rsa_public_key_to_der(const RSA_PUBLIC_KEY *key, uint8_t **out, size_t *outlen);
int rsa_public_key_from_der(RSA_PUBLIC_KEY *key, const uint8_t **in, size_t *inlen);
int rsa_public_key_operation(const RSA_PUBLIC_KEY *key,
	const uint8_t *in, size_t inlen,
	uint8_t *out, size_t outmax, size_t *outlen);
int rsa_verify_pkcs1_v15_sha256(const RSA_PUBLIC_KEY *key,
	const uint8_t dgst[32], const uint8_t *sig, size_t siglen);
int rsa_verify_pss_sha256(const RSA_PUBLIC_KEY *key,
	const uint8_t dgst[32], const uint8_t *sig, size_t siglen);
int rsa_public_key_print(FILE *fp, int fmt, int ind, const char *label, const uint8_t *d, size_t dlen);






#ifdef __cplusplus
}
#endif
#endif
