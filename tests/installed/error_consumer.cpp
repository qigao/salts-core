#include <cmeta_error.h>

#include <climits>
#include <cstdio>
#include <cstring>
#include <type_traits>

#define REQUIRE(condition) do { \
  if (!(condition)) { \
    std::fprintf(stderr, "installed error C++17 contract failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

static_assert(std::is_enum<cmeta_error_domain_t>::value, "native domain remains an enum");
static_assert(std::is_standard_layout<cmeta_error_info_t>::value, "plain C metadata layout");
static_assert(std::is_same<decltype(&cmeta_strerror), const char *(*)(int)>::value,
              "original C function signature");

int main() {
  const cmeta_error_entry_t entry{SALTS_ERROR_CUSTOM(72, 65535), "CPP_ERROR", "C++ storage"};
  const cmeta_error_domain_desc_t descriptor{72, "cpp", &entry, 1};
  cmeta_error_domain_t parsed = SALTS_ERROR_DOMAIN_POSIX;
  REQUIRE(cmeta_error_domain_count() == 6);
  REQUIRE(cmeta_error_domain_from_string("win32", &parsed) == 0);
  REQUIRE(cmeta_error_domain_equals(parsed, SALTS_ERROR_DOMAIN_WIN32));
  REQUIRE(cmeta_error_domain_is_valid(parsed));
  REQUIRE(std::strcmp(cmeta_error_domain_to_string(parsed), "win32") == 0);
  REQUIRE(cmeta_error_domain_from_string(nullptr, &parsed) == -1);
  REQUIRE(parsed == SALTS_ERROR_DOMAIN_WIN32);
  REQUIRE(cmeta_error_register_domain(&descriptor) == SALTS_OK);
  const auto info = cmeta_error_info(entry.code);
  REQUIRE(info.domain == SALTS_ERROR_DOMAIN_CUSTOM && info.custom_domain == descriptor.domain);
  REQUIRE(info.name == entry.name && info.message == entry.message);
  REQUIRE(cmeta_strerror(entry.code) == entry.message);
  REQUIRE(cmeta_error_unregister_domain(descriptor.domain) == SALTS_OK);
  REQUIRE(cmeta_error_info(entry.code).domain == SALTS_ERROR_DOMAIN_UNKNOWN);
  const auto result = cmeta_result_from_code(SALTS_EINVAL);
  REQUIRE(cmeta_result_is_err(result) && !cmeta_result_is_ok(result));
  REQUIRE(result.code == SALTS_EINVAL && std::strcmp(result.message, "invalid argument") == 0);
  REQUIRE(cmeta_result_is_ok(cmeta_result_ok()));
  char unknown[64];
  std::snprintf(unknown, sizeof(unknown), "unknown error %d", INT_MIN);
  REQUIRE(std::strcmp(cmeta_strerror(INT_MIN), unknown) == 0);
  return 0;
}
