#ifndef CRYPTO_INTERNAL_H_INCLUDED
#define CRYPTO_INTERNAL_H_INCLUDED

#include <stddef.h>

/* Single Crypto owner; OS CSPRNG has no platform/CFlow dependency. */
int salts_crypto_random_bytes(void *buffer, size_t size);

static inline void salts_crypto_secure_zero(void *buffer, size_t size) {
  volatile unsigned char *p = (volatile unsigned char *)buffer;
  while (size-- != 0u) {
    *p++ = 0u;
  }
}

#endif /* CRYPTO_INTERNAL_H_INCLUDED */
