#include <tstr.h>
#include <str.h>
#include <memory_pool.h>
#include <string.h>
int main(void) {
  tstr s = tstr_dup("Core");
  if (!s) return 1;
  if (!tstr_append(&s, VSTR_LIT(" SDK"))) { tstr_free(s); return 2; }
  int ok = tstr_len(s) == 8 && strcmp(s, "Core SDK") == 0 &&
           vstr_eq(tstr_view(s), VSTR_LIT("Core SDK"));
  MemoryPool *pool = pool_create(256);
  if (!pool) { tstr_free(s); return 3; }
  char *copied = vstr_to_pool(tstr_view(s), pool);
  ok = ok && copied && strcmp(copied, s) == 0;
  pool_destroy(pool);
  tstr_free(s);
  return ok ? 0 : 4;
}
