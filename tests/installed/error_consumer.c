#include <cmeta_error.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "installed error C11 contract failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

_Static_assert(SALTS_ERROR_DOMAIN_NONE == 0 && SALTS_ERROR_DOMAIN_UNKNOWN == 5,
               "native domain values remain stable");

int main(void) {
  static const cmeta_error_entry_t entries[] = {
    {SALTS_ERROR_CUSTOM(71, 1), "SDK_ERROR", "borrowed SDK message"}
  };
  const cmeta_error_domain_desc_t domain = {71, "SDK", entries, 1};
  cmeta_error_domain_t parsed = SALTS_ERROR_DOMAIN_NONE;
  cmeta_error_info_t info;
  cmeta_result_t result;
  char unknown[64];
  REQUIRE(cmeta_error_domain_count() == 6);
  REQUIRE(cmeta_error_domain_from_string("custom", &parsed) == 0);
  REQUIRE(cmeta_error_domain_equals(parsed, SALTS_ERROR_DOMAIN_CUSTOM));
  REQUIRE(cmeta_error_domain_is_valid(parsed));
  REQUIRE(strcmp(cmeta_error_domain_to_string(parsed), "custom") == 0);
  REQUIRE(cmeta_error_domain_from_string("SALTS_ERROR_DOMAIN_CUSTOM", &parsed) == -1);
  REQUIRE(parsed == SALTS_ERROR_DOMAIN_CUSTOM);
  REQUIRE(cmeta_error_register_domain(&domain) == SALTS_OK);
  info = cmeta_error_info(entries[0].code);
  REQUIRE(info.domain == SALTS_ERROR_DOMAIN_CUSTOM && info.custom_domain == 71);
  REQUIRE(info.domain_name == domain.domain_name && info.name == entries[0].name);
  REQUIRE(info.message == entries[0].message && cmeta_strerror(entries[0].code) == entries[0].message);
  REQUIRE(cmeta_error_unregister_domain(71) == SALTS_OK);
  REQUIRE(cmeta_error_info(entries[0].code).domain == SALTS_ERROR_DOMAIN_UNKNOWN);
  result = cmeta_result_from_code(SALTS_ENOMEM);
  REQUIRE(cmeta_result_is_err(result) && !cmeta_result_is_ok(result));
  REQUIRE(result.code == SALTS_ENOMEM && strcmp(result.message, "not enough memory") == 0);
  REQUIRE(cmeta_result_is_ok(cmeta_result_ok()));
  snprintf(unknown, sizeof(unknown), "unknown error %d", INT_MIN);
  REQUIRE(strcmp(cmeta_strerror(INT_MIN), unknown) == 0);
  return 0;
}
