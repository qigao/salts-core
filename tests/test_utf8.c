#include <salts_core/utf8.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) return __LINE__; } while (0)

static int valid_bytes(const unsigned char *p, size_t n) {
  return salts_core_utf8_valid((const char *)p, n);
}

int main(void) {
  static const unsigned char ascii[] = {'a', 0u, 'b', '/', 'c'};
  static const unsigned char two[] = {0xc2u, 0xa2u};
  static const unsigned char three[] = {0xe6u, 0x97u, 0xa5u};
  static const unsigned char four[] = {0xf0u, 0x9fu, 0x8eu, 0x89u};
  static const unsigned char maxcp[] = {0xf4u, 0x8fu, 0xbfu, 0xbfu};
  static const unsigned char mincp[] = {0xf0u, 0x90u, 0x80u, 0x80u};
  static const unsigned char shortest3[] = {0xe0u, 0xa0u, 0x80u};
  static const unsigned char invalid[][4] = {
    {0x80u, 0x00u, 0x00u, 0x00u}, /* stray continuation */
    {0xc0u, 0x80u, 0x00u, 0x00u}, /* overlong ASCII */
    {0xc1u, 0xbfu, 0x00u, 0x00u},
    {0xe0u, 0x9fu, 0xbfu, 0x00u}, /* overlong three-byte */
    {0xedu, 0xa0u, 0x80u, 0x00u}, /* surrogate D800 */
    {0xedu, 0xbfu, 0xbfu, 0x00u}, /* surrogate DFFF */
    {0xf0u, 0x8fu, 0xbfu, 0xbfu}, /* overlong four-byte */
    {0xf4u, 0x90u, 0x80u, 0x80u}, /* >U+10FFFF */
    {0xf5u, 0x80u, 0x80u, 0x80u},
    {0xffu, 0x00u, 0x00u, 0x00u},
    {0xc2u, 0x41u, 0x00u, 0x00u}, /* wrong continuation */
    {0xe2u, 0x82u, 0x41u, 0x00u},
    {0xf0u, 0x9fu, 0x90u, 0x41u}
  };
  static const size_t badlen[] = {1,2,2,3,3,3,4,4,4,1,2,3,4};
  size_t i;
  unsigned b;

  CHECK(salts_core_utf8_valid(NULL, 0u) == 1);
  CHECK(salts_core_utf8_valid(NULL, 1u) == 0);
  CHECK(salts_core_utf8_valid("", 0u) == 1);
  CHECK(valid_bytes(ascii, sizeof(ascii)) == 1);
  CHECK(valid_bytes(two, sizeof(two)) == 1);
  CHECK(valid_bytes(three, sizeof(three)) == 1);
  CHECK(valid_bytes(four, sizeof(four)) == 1);
  CHECK(valid_bytes(maxcp, sizeof(maxcp)) == 1);
  CHECK(valid_bytes(mincp, sizeof(mincp)) == 1);
  CHECK(valid_bytes(shortest3, sizeof(shortest3)) == 1);
  for (i = 0u; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
    CHECK(valid_bytes(invalid[i], badlen[i]) == 0);
  }
  for (i = 1u; i < sizeof(two); ++i) CHECK(valid_bytes(two,i) == 0);
  for (i = 1u; i < sizeof(three); ++i) CHECK(valid_bytes(three,i) == 0);
  for (i = 1u; i < sizeof(four); ++i) CHECK(valid_bytes(four,i) == 0);

  /* Check the standalone one-byte alphabet, including NUL, as vstr does. */
  for (b = 0u; b < 256u; ++b) {
    unsigned char one[1];
    one[0] = (unsigned char)b;
    CHECK(valid_bytes(one,1u) == (b < 0x80u ? 1 : 0));
  }
  return 0;
}
