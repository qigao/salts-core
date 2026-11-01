#include <fastpath.h>
#include <error_codes.h>
#include <stdio.h>

int main(void) {
  SALTS_FAST_KEY(key, false);
  if (salts_fast_branch(&key)) return 1;
  if (salts_fast_enable(&key) != SALTS_OK) return 2;
  if (!salts_fast_branch(&key) || !salts_fast_key_read(&key)) return 3;
  if (!salts_fast_key_consume(&key) || salts_fast_key_read(&key)) return 4;
  if (salts_fast_disable(&key) != SALTS_OK) return 5;
  if (salts_fast_key_set(&key, true) != SALTS_OK) return 6;
  if (salts_fast_key_set(NULL, true) != SALTS_EINVAL) return 7;
  if (salts_fast_key_consume(NULL)) return 8;
  if (!salts_fast_key_read(&key)) return 9;
  puts("Native Platform fast-key owner: PASS");
  return 0;
}
