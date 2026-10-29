/*
 *  Copyright 2014-2022 The GmSSL Project. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the License); you may
 *  not use this file except in compliance with the License.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <gmssl/rsa.h>
#include <gmssl/bn.h>
#include <gmssl/mem.h>
#include <gmssl/sha2.h>
#include <gmssl/rand.h>
#include <gmssl/asn1.h>
#include <gmssl/pem.h>
#include <gmssl/x509_alg.h>
#include <gmssl/error.h>


int rsa_public_key_to_der(const RSA_PUBLIC_KEY *key, uint8_t **out, size_t *outlen)
{
	size_t len = 0;

	if (!key || !outlen
		|| key->modulus_size < RSA_MIN_MODULUS_SIZE
		|| key->modulus_size > RSA_MAX_MODULUS_SIZE
		|| (key->modulus_size & 3u) != 0
		|| key->modulus[0] == 0
		|| (key->modulus[key->modulus_size - 1] & 1u) == 0
		|| key->public_exponent < 3
		|| key->public_exponent > 0x7fffffffu
		|| (key->public_exponent & 1u) == 0) {
		error_print();
		return -1;
	}
	if (asn1_integer_to_der(key->modulus, key->modulus_size, NULL, &len) != 1
		|| asn1_int_to_der((int)key->public_exponent, NULL, &len) != 1
		|| asn1_sequence_header_to_der(len, out, outlen) != 1
		|| asn1_integer_to_der(key->modulus, key->modulus_size, out, outlen) != 1
		|| asn1_int_to_der((int)key->public_exponent, out, outlen) != 1) {
		error_print();
		return -1;
	}
	return 1;
}

int rsa_public_key_from_der(RSA_PUBLIC_KEY *key, const uint8_t **in, size_t *inlen)
{
	const uint8_t *d;
	size_t dlen;
	const uint8_t *modulus;
	size_t modulus_len;
	int exponent;

	if (!key || !in || !inlen) {
		error_print();
		return -1;
	}
	memset(key, 0, sizeof(*key));
	if (asn1_sequence_from_der(&d, &dlen, in, inlen) != 1
		|| asn1_integer_from_der(&modulus, &modulus_len, &d, &dlen) != 1
		|| asn1_int_from_der(&exponent, &d, &dlen) != 1
		|| asn1_length_is_zero(dlen) != 1) {
		error_print();
		return -1;
	}
	/* DER INTEGER may carry one sign-padding zero for a positive modulus. */
	if (modulus_len > 1 && modulus[0] == 0) {
		modulus++;
		modulus_len--;
	}
	if (modulus_len < RSA_MIN_MODULUS_SIZE
		|| modulus_len > RSA_MAX_MODULUS_SIZE
		|| (modulus_len & 3u) != 0
		|| modulus[0] == 0
		|| (modulus[modulus_len - 1] & 1u) == 0
		|| exponent < 3
		|| (exponent & 1) == 0) {
		error_print();
		return -1;
	}
	key->modulus_size = modulus_len;
	memcpy(key->modulus, modulus, modulus_len);
	key->public_exponent = (uint32_t)exponent;
	return 1;
}

static int rsa_mgf1_sha256(const uint8_t *seed, size_t seedlen, uint8_t *out, size_t outlen)
{
	uint32_t counter = 0;

	if ((!seed && seedlen) || (!out && outlen)) {
		error_print();
		return -1;
	}
	while (outlen) {
		SHA256_CTX ctx;
		uint8_t block[SHA256_DIGEST_SIZE];
		uint8_t count[4];
		size_t n = outlen < sizeof(block) ? outlen : sizeof(block);

		count[0] = (uint8_t)(counter >> 24);
		count[1] = (uint8_t)(counter >> 16);
		count[2] = (uint8_t)(counter >> 8);
		count[3] = (uint8_t)counter;
		sha256_init(&ctx);
		sha256_update(&ctx, seed, seedlen);
		sha256_update(&ctx, count, sizeof(count));
		sha256_finish(&ctx, block);
		memcpy(out, block, n);
		out += n;
		outlen -= n;
		counter++;
		gmssl_secure_clear(&ctx, sizeof(ctx));
		gmssl_secure_clear(block, sizeof(block));
	}
	return 1;
}

static size_t rsa_modulus_bits(const RSA_PUBLIC_KEY *key)
{
	uint8_t b;
	size_t bits;

	if (!key || !key->modulus_size || key->modulus[0] == 0) return 0;
	b = key->modulus[0];
	bits = (key->modulus_size - 1) * 8;
	while (b) {
		bits++;
		b >>= 1;
	}
	return bits;
}

int rsa_verify_pkcs1_v15_sha256(const RSA_PUBLIC_KEY *key,
	const uint8_t dgst[32], const uint8_t *sig, size_t siglen)
{
	static const uint8_t digest_info_prefix[] = {
		0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,
		0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20
	};
	uint8_t em[RSA_MAX_MODULUS_SIZE];
	size_t emlen = 0;
	size_t i;
	int ret = 0;

	if (!key || !dgst || !sig || siglen != key->modulus_size) {
		error_print();
		return -1;
	}
	if (rsa_public_key_operation(key, sig, siglen, em, sizeof(em), &emlen) != 1) {
		return 0;
	}
	if (emlen < 3 + 8 + sizeof(digest_info_prefix) + SHA256_DIGEST_SIZE
		|| em[0] != 0x00 || em[1] != 0x01) {
		goto end;
	}
	i = 2;
	while (i < emlen && em[i] == 0xff) i++;
	if (i < 10 || i >= emlen || em[i] != 0x00) goto end;
	i++;
	if (emlen - i != sizeof(digest_info_prefix) + SHA256_DIGEST_SIZE) goto end;
	if (gmssl_secure_memcmp(em + i, digest_info_prefix, sizeof(digest_info_prefix)) != 0) goto end;
	i += sizeof(digest_info_prefix);
	if (gmssl_secure_memcmp(em + i, dgst, SHA256_DIGEST_SIZE) != 0) goto end;
	ret = 1;

end:
	gmssl_secure_clear(em, sizeof(em));
	return ret;
}

int rsa_verify_pss_sha256(const RSA_PUBLIC_KEY *key,
	const uint8_t dgst[32], const uint8_t *sig, size_t siglen)
{
	enum { SALT_LEN = SHA256_DIGEST_SIZE };
	uint8_t recovered[RSA_MAX_MODULUS_SIZE];
	uint8_t db[RSA_MAX_MODULUS_SIZE];
	uint8_t mask[RSA_MAX_MODULUS_SIZE];
	uint8_t expected_h[SHA256_DIGEST_SIZE];
	uint8_t prefix[8] = {0};
	SHA256_CTX hash_ctx;
	size_t recovered_len = 0;
	size_t mod_bits;
	size_t em_bits;
	size_t em_len;
	size_t offset;
	size_t db_len;
	size_t ps_len;
	size_t unused_bits;
	const uint8_t *em;
	const uint8_t *h;
	const uint8_t *salt;
	size_t i;
	int ret = 0;

	if (!key || !dgst || !sig || siglen != key->modulus_size) {
		error_print();
		return -1;
	}
	mod_bits = rsa_modulus_bits(key);
	if (mod_bits < 2) return -1;
	em_bits = mod_bits - 1;
	em_len = (em_bits + 7) / 8;
	if (em_len < SHA256_DIGEST_SIZE + SALT_LEN + 2
		|| key->modulus_size < em_len
		|| key->modulus_size - em_len > 1) {
		error_print();
		return -1;
	}
	if (rsa_public_key_operation(key, sig, siglen,
			recovered, sizeof(recovered), &recovered_len) != 1) {
		return 0;
	}
	if (recovered_len != key->modulus_size) goto end;
	offset = recovered_len - em_len;
	for (i = 0; i < offset; i++) {
		if (recovered[i] != 0) goto end;
	}
	em = recovered + offset;
	if (em[em_len - 1] != 0xbc) goto end;

	db_len = em_len - SHA256_DIGEST_SIZE - 1;
	h = em + db_len;
	unused_bits = 8 * em_len - em_bits;
	if (unused_bits > 7) goto end;
	if (unused_bits && (em[0] & (uint8_t)(0xffu << (8 - unused_bits))) != 0) goto end;

	if (rsa_mgf1_sha256(h, SHA256_DIGEST_SIZE, mask, db_len) != 1) goto end;
	for (i = 0; i < db_len; i++) db[i] = em[i] ^ mask[i];
	if (unused_bits) db[0] &= (uint8_t)(0xffu >> unused_bits);

	ps_len = em_len - SHA256_DIGEST_SIZE - SALT_LEN - 2;
	for (i = 0; i < ps_len; i++) {
		if (db[i] != 0) goto end;
	}
	if (db[ps_len] != 0x01) goto end;
	salt = db + ps_len + 1;

	sha256_init(&hash_ctx);
	sha256_update(&hash_ctx, prefix, sizeof(prefix));
	sha256_update(&hash_ctx, dgst, SHA256_DIGEST_SIZE);
	sha256_update(&hash_ctx, salt, SALT_LEN);
	sha256_finish(&hash_ctx, expected_h);
	if (gmssl_secure_memcmp(h, expected_h, SHA256_DIGEST_SIZE) != 0) goto end;
	ret = 1;

end:
	gmssl_secure_clear(recovered, sizeof(recovered));
	gmssl_secure_clear(db, sizeof(db));
	gmssl_secure_clear(mask, sizeof(mask));
	gmssl_secure_clear(expected_h, sizeof(expected_h));
	gmssl_secure_clear(&hash_ctx, sizeof(hash_ctx));
	return ret;
}

static int rsa_private_component_copy(uint8_t *out, size_t outlen,
	const uint8_t *in, size_t inlen)
{
	if (!out || !outlen || !in || !inlen) {
		error_print();
		return -1;
	}
	if (inlen > 1 && in[0] == 0) {
		in++;
		inlen--;
	}
	if (!inlen || inlen > outlen) {
		error_print();
		return -1;
	}
	memset(out, 0, outlen);
	memcpy(out + outlen - inlen, in, inlen);
	return 1;
}

int rsa_private_key_from_der(RSA_PRIVATE_KEY *key, const uint8_t **in, size_t *inlen)
{
	const uint8_t *d;
	size_t dlen;
	int version;
	const uint8_t *n;
	size_t nlen;
	int e;
	const uint8_t *private_exponent;
	size_t private_exponent_len;
	const uint8_t *p;
	size_t plen;
	const uint8_t *q;
	size_t qlen;
	const uint8_t *dP;
	size_t dPlen;
	const uint8_t *dQ;
	size_t dQlen;
	const uint8_t *qInv;
	size_t qInvlen;
	uint32_t p_bn[RSA_MAX_PRIME_SIZE / 4];
	uint32_t q_bn[RSA_MAX_PRIME_SIZE / 4];
	uint32_t product[RSA_MAX_MODULUS_WORDS];
	uint32_t exponent1_bn[RSA_MAX_PRIME_SIZE / 4];
	uint32_t exponent2_bn[RSA_MAX_PRIME_SIZE / 4];
	uint32_t coefficient_bn[RSA_MAX_PRIME_SIZE / 4];
	uint8_t product_bytes[RSA_MAX_MODULUS_SIZE];
	size_t k;
	int ret = -1;

	if (!key || !in || !*in || !inlen) {
		error_print();
		return -1;
	}
	memset(key, 0, sizeof(*key));
	memset(p_bn, 0, sizeof(p_bn));
	memset(q_bn, 0, sizeof(q_bn));
	memset(product, 0, sizeof(product));
	memset(exponent1_bn, 0, sizeof(exponent1_bn));
	memset(exponent2_bn, 0, sizeof(exponent2_bn));
	memset(coefficient_bn, 0, sizeof(coefficient_bn));
	memset(product_bytes, 0, sizeof(product_bytes));

	if (asn1_sequence_from_der(&d, &dlen, in, inlen) != 1
		|| asn1_int_from_der(&version, &d, &dlen) != 1
		|| asn1_integer_from_der(&n, &nlen, &d, &dlen) != 1
		|| asn1_int_from_der(&e, &d, &dlen) != 1
		|| asn1_integer_from_der(&private_exponent, &private_exponent_len, &d, &dlen) != 1
		|| asn1_integer_from_der(&p, &plen, &d, &dlen) != 1
		|| asn1_integer_from_der(&q, &qlen, &d, &dlen) != 1
		|| asn1_integer_from_der(&dP, &dPlen, &d, &dlen) != 1
		|| asn1_integer_from_der(&dQ, &dQlen, &d, &dlen) != 1
		|| asn1_integer_from_der(&qInv, &qInvlen, &d, &dlen) != 1
		|| asn1_length_is_zero(dlen) != 1) {
		error_print();
		goto end;
	}
	if (version != 0) {
		error_print();
		goto end;
	}
	if (nlen > 1 && n[0] == 0) {
		n++;
		nlen--;
	}
	if (nlen < RSA_MIN_MODULUS_SIZE
		|| nlen > RSA_MAX_MODULUS_SIZE
		|| (nlen & 7u) != 0
		|| n[0] == 0
		|| (n[nlen - 1] & 1u) == 0
		|| e < 3 || (e & 1) == 0
		|| private_exponent_len == 0 || private_exponent_len > nlen + 1) {
		error_print();
		goto end;
	}

	key->public_key.modulus_size = nlen;
	memcpy(key->public_key.modulus, n, nlen);
	key->public_key.public_exponent = (uint32_t)e;
	key->prime_size = nlen / 2;

	if (rsa_private_component_copy(key->prime1, key->prime_size, p, plen) != 1
		|| rsa_private_component_copy(key->prime2, key->prime_size, q, qlen) != 1
		|| rsa_private_component_copy(key->exponent1, key->prime_size, dP, dPlen) != 1
		|| rsa_private_component_copy(key->exponent2, key->prime_size, dQ, dQlen) != 1
		|| rsa_private_component_copy(key->coefficient, key->prime_size, qInv, qInvlen) != 1) {
		error_print();
		goto end;
	}
	if ((key->prime1[key->prime_size - 1] & 1u) == 0
		|| (key->prime2[key->prime_size - 1] & 1u) == 0
		|| key->prime1[0] == 0 || key->prime2[0] == 0) {
		error_print();
		goto end;
	}

	k = key->prime_size / 4;
	bn_from_bytes(p_bn, k, key->prime1);
	bn_from_bytes(q_bn, k, key->prime2);
	bn_from_bytes(exponent1_bn, k, key->exponent1);
	bn_from_bytes(exponent2_bn, k, key->exponent2);
	bn_from_bytes(coefficient_bn, k, key->coefficient);
	if (bn_cmp(p_bn, q_bn, k) == 0
		|| bn_is_zero(exponent1_bn, k)
		|| bn_cmp(exponent1_bn, p_bn, k) >= 0
		|| bn_is_zero(exponent2_bn, k)
		|| bn_cmp(exponent2_bn, q_bn, k) >= 0
		|| bn_is_zero(coefficient_bn, k)
		|| bn_cmp(coefficient_bn, p_bn, k) >= 0) {
		error_print();
		goto end;
	}
	bn_mul(product, p_bn, q_bn, k);
	bn_to_bytes(product, k * 2, product_bytes);
	if (gmssl_secure_memcmp(product_bytes,
		key->public_key.modulus, key->public_key.modulus_size) != 0) {
		error_print();
		goto end;
	}

	ret = 1;

end:
	if (ret != 1) {
		gmssl_secure_clear(key, sizeof(*key));
	}
	gmssl_secure_clear(p_bn, sizeof(p_bn));
	gmssl_secure_clear(q_bn, sizeof(q_bn));
	gmssl_secure_clear(product, sizeof(product));
	gmssl_secure_clear(exponent1_bn, sizeof(exponent1_bn));
	gmssl_secure_clear(exponent2_bn, sizeof(exponent2_bn));
	gmssl_secure_clear(coefficient_bn, sizeof(coefficient_bn));
	gmssl_secure_clear(product_bytes, sizeof(product_bytes));
	return ret;
}

int rsa_private_key_info_from_der(RSA_PRIVATE_KEY *key,
	const uint8_t **attrs, size_t *attrslen,
	const uint8_t **in, size_t *inlen)
{
	const uint8_t *d;
	size_t dlen;
	int version;
	int algor;
	int algor_param;
	const uint8_t *private_key;
	size_t private_key_len;
	const uint8_t *local_attrs = NULL;
	size_t local_attrs_len = 0;
	const uint8_t *p;
	size_t len;

	if (attrs) *attrs = NULL;
	if (attrslen) *attrslen = 0;
	if (!key || !in || !*in || !inlen) {
		error_print();
		return -1;
	}
	if (asn1_sequence_from_der(&d, &dlen, in, inlen) != 1
		|| asn1_int_from_der(&version, &d, &dlen) != 1
		|| x509_public_key_algor_from_der(&algor, &algor_param, &d, &dlen) != 1
		|| asn1_octet_string_from_der(&private_key, &private_key_len, &d, &dlen) != 1
		|| asn1_implicit_set_from_der(0, &local_attrs, &local_attrs_len, &d, &dlen) < 0
		|| asn1_length_is_zero(dlen) != 1) {
		error_print();
		return -1;
	}
	if (version != 0 || algor != OID_rsa_encryption || algor_param != OID_undef
		|| !private_key || !private_key_len) {
		error_print();
		return -1;
	}
	p = private_key;
	len = private_key_len;
	if (rsa_private_key_from_der(key, &p, &len) != 1
		|| asn1_length_is_zero(len) != 1) {
		error_print();
		return -1;
	}
	if (attrs) *attrs = local_attrs;
	if (attrslen) *attrslen = local_attrs_len;
	return 1;
}

int rsa_private_key_info_from_pem(RSA_PRIVATE_KEY *key, FILE *fp)
{
	enum { RSA_PKCS8_MAX_DER_SIZE = 4096 };
	uint8_t buf[RSA_PKCS8_MAX_DER_SIZE];
	const uint8_t *p = buf;
	size_t len = 0;
	const uint8_t *attrs = NULL;
	size_t attrslen = 0;
	int ret = -1;

	if (!key || !fp) {
		error_print();
		return -1;
	}
	if (pem_read(fp, "PRIVATE KEY", buf, &len, sizeof(buf)) != 1
		|| rsa_private_key_info_from_der(key, &attrs, &attrslen, &p, &len) != 1
		|| asn1_length_is_zero(len) != 1
		|| attrslen != 0) {
		error_print();
		goto end;
	}
	ret = 1;

end:
	gmssl_secure_clear(buf, sizeof(buf));
	if (ret != 1) {
		rsa_private_key_cleanup(key);
	}
	return ret;
}

void rsa_private_key_cleanup(RSA_PRIVATE_KEY *key)
{
	if (key) {
		gmssl_secure_clear(key, sizeof(*key));
	}
}

int rsa_sign_pkcs1_v15_sha256(const RSA_PRIVATE_KEY *key,
	const uint8_t dgst[32], uint8_t *sig, size_t sigmax, size_t *siglen)
{
	static const uint8_t digest_info_prefix[] = {
		0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,
		0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20
	};
	uint8_t em[RSA_MAX_MODULUS_SIZE];
	size_t emlen;
	size_t pslen;
	int ret;

	if (siglen) *siglen = 0;
	if (!key || !dgst || !sig || !siglen
		|| key->public_key.modulus_size < RSA_MIN_MODULUS_SIZE
		|| key->public_key.modulus_size > RSA_MAX_MODULUS_SIZE
		|| sigmax < key->public_key.modulus_size) {
		error_print();
		return -1;
	}
	emlen = key->public_key.modulus_size;
	if (emlen < 3 + 8 + sizeof(digest_info_prefix) + SHA256_DIGEST_SIZE) {
		error_print();
		return -1;
	}
	pslen = emlen - 3 - sizeof(digest_info_prefix) - SHA256_DIGEST_SIZE;
	em[0] = 0x00;
	em[1] = 0x01;
	memset(em + 2, 0xff, pslen);
	em[2 + pslen] = 0x00;
	memcpy(em + 3 + pslen, digest_info_prefix, sizeof(digest_info_prefix));
	memcpy(em + 3 + pslen + sizeof(digest_info_prefix), dgst, SHA256_DIGEST_SIZE);

	ret = rsa_private_key_operation(key, em, emlen, sig, sigmax, siglen);
	gmssl_secure_clear(em, sizeof(em));
	return ret;
}

int rsa_sign_pss_sha256_with_salt(const RSA_PRIVATE_KEY *key,
	const uint8_t dgst[32], const uint8_t salt[32],
	uint8_t *sig, size_t sigmax, size_t *siglen)
{
	enum { SALT_LEN = SHA256_DIGEST_SIZE };
	uint8_t em[RSA_MAX_MODULUS_SIZE];
	uint8_t db[RSA_MAX_MODULUS_SIZE];
	uint8_t mask[RSA_MAX_MODULUS_SIZE];
	uint8_t h[SHA256_DIGEST_SIZE];
	uint8_t prefix[8] = {0};
	SHA256_CTX hash_ctx;
	size_t mod_bits;
	size_t em_bits;
	size_t em_len;
	size_t db_len;
	size_t ps_len;
	size_t unused_bits;
	size_t offset;
	size_t i;
	int ret = -1;

	if (siglen) *siglen = 0;
	if (!key || !dgst || !salt || !sig || !siglen
		|| key->public_key.modulus_size < RSA_MIN_MODULUS_SIZE
		|| key->public_key.modulus_size > RSA_MAX_MODULUS_SIZE
		|| sigmax < key->public_key.modulus_size) {
		error_print();
		return -1;
	}
	mod_bits = rsa_modulus_bits(&key->public_key);
	if (mod_bits < 2) {
		error_print();
		return -1;
	}
	em_bits = mod_bits - 1;
	em_len = (em_bits + 7) / 8;
	if (em_len < SHA256_DIGEST_SIZE + SALT_LEN + 2
		|| key->public_key.modulus_size < em_len
		|| key->public_key.modulus_size - em_len > 1) {
		error_print();
		return -1;
	}
	memset(em, 0, sizeof(em));
	memset(db, 0, sizeof(db));
	memset(mask, 0, sizeof(mask));
	memset(h, 0, sizeof(h));
	memset(&hash_ctx, 0, sizeof(hash_ctx));

	sha256_init(&hash_ctx);
	sha256_update(&hash_ctx, prefix, sizeof(prefix));
	sha256_update(&hash_ctx, dgst, SHA256_DIGEST_SIZE);
	sha256_update(&hash_ctx, salt, SALT_LEN);
	sha256_finish(&hash_ctx, h);

	db_len = em_len - SHA256_DIGEST_SIZE - 1;
	ps_len = em_len - SHA256_DIGEST_SIZE - SALT_LEN - 2;
	memset(db, 0, ps_len);
	db[ps_len] = 0x01;
	memcpy(db + ps_len + 1, salt, SALT_LEN);
	if (rsa_mgf1_sha256(h, sizeof(h), mask, db_len) != 1) {
		error_print();
		goto end;
	}
	for (i = 0; i < db_len; i++) {
		db[i] ^= mask[i];
	}
	unused_bits = 8 * em_len - em_bits;
	if (unused_bits) {
		db[0] &= (uint8_t)(0xffu >> unused_bits);
	}

	offset = key->public_key.modulus_size - em_len;
	memcpy(em + offset, db, db_len);
	memcpy(em + offset + db_len, h, sizeof(h));
	em[offset + em_len - 1] = 0xbc;

	ret = rsa_private_key_operation(key, em, key->public_key.modulus_size,
		sig, sigmax, siglen);

end:
	gmssl_secure_clear(em, sizeof(em));
	gmssl_secure_clear(db, sizeof(db));
	gmssl_secure_clear(mask, sizeof(mask));
	gmssl_secure_clear(h, sizeof(h));
	gmssl_secure_clear(&hash_ctx, sizeof(hash_ctx));
	return ret;
}

int rsa_sign_pss_sha256(const RSA_PRIVATE_KEY *key,
	const uint8_t dgst[32], uint8_t *sig, size_t sigmax, size_t *siglen)
{
	uint8_t salt[SHA256_DIGEST_SIZE];
	int ret;

	if (rand_bytes(salt, sizeof(salt)) != 1) {
		error_print();
		return -1;
	}
	ret = rsa_sign_pss_sha256_with_salt(key, dgst, salt, sig, sigmax, siglen);
	gmssl_secure_clear(salt, sizeof(salt));
	return ret;
}

int rsa_public_key_print(FILE *fp, int fmt, int ind, const char *label, const uint8_t *a, size_t alen)
{


	const uint8_t *d;
	size_t dlen;
	const uint8_t *p;
	size_t len;
	int val;

	format_print(fp, fmt, ind, "%s\n", label);
	ind += 4;
	if (asn1_sequence_from_der(&d, &dlen, &a, &alen) != 1) goto err;
	if (asn1_integer_from_der(&p, &len, &d, &dlen) != 1) goto err;
	format_bytes(fp, fmt, ind, "modulus", p, len);
	if (asn1_int_from_der(&val, &d, &dlen) != 1) goto err;
	format_print(fp, fmt, ind, "publicExponent: %d\n",val);
	if (asn1_length_is_zero(dlen) != 1) goto err;
	if (asn1_length_is_zero(alen) != 1) goto err;
	return 1;
err:
	error_print();
	return -1;
}
