#include <base64_utils.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

int main() {
  const std::uint8_t input[] = {0u, 12u, 254u, 255u};
  tn_base64_string_result_t encoded = tn_base64_encode_ex(input, sizeof(input));
  if (!encoded.ok || !encoded.value) return 1;
  tn_base64_bytes_result_t decoded = tn_base64_decode_ex(encoded.value);
  std::free(encoded.value);
  if (!decoded.ok || decoded.value.len != sizeof(input) ||
      std::memcmp(input, decoded.value.data, sizeof(input)) != 0) return 2;
  std::free(decoded.value.data);
  if (tn_base64_decode_ex("a=").ok) return 3;
  return 0;
}
