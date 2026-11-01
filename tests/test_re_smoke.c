/* Original bounded regex engine: no TinyTest, CMeta, CFlow or SIMDe.
 * Independent C11 runtime acceptance in the default Core build.
 * The original 25 TinyTest cases are retained separately in test_re.c.
 */
#include "re.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "regex smoke FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

int main(void)
{
    re_t compiled = NULL;
    re_match_result_t result = {RE_NPOS, 0u};
    re_limits_t limits = re_limits_default();
    static const char binary[] = {'x', '\0', 'a', 'a', 'b', 'y'};
    static const char no_match[] = "xxxxxxxxxxxxxxxxxxxx";
    int matched = -1;

    REQUIRE(limits.struct_size == sizeof(limits));
    REQUIRE(limits.max_pattern_bytes > 1u);
    REQUIRE(limits.max_text_bytes > 4u);
    REQUIRE(limits.max_steps > 0u);
    REQUIRE(re_status_string(RE_STATUS_NO_MATCH) != NULL);
    REQUIRE(re_status_string(RE_STATUS_NO_MATCH)[0] != '\0');
    REQUIRE(re_validate_n("(a|b", 4u, NULL) == RE_STATUS_INVALID_PATTERN);
    REQUIRE(re_compile_n("a+b", 3u, NULL, &compiled) == RE_STATUS_OK);
    REQUIRE(compiled != NULL);
    REQUIRE(re_matchn(compiled, binary, sizeof(binary), NULL, &result)
            == RE_STATUS_OK);
    REQUIRE(result.index == 2u && result.length == 3u);
    re_destroy(compiled);
    compiled = NULL;

    REQUIRE(re_match_n("a+b", 3u, binary, sizeof(binary), NULL, &result)
            == RE_STATUS_OK);
    REQUIRE(result.index == 2u && result.length == 3u);
    REQUIRE(re_match("(cat|dog)", "xxdogyy", &matched) == 2);
    REQUIRE(matched == 3);

    limits.max_text_bytes = 4u;
    REQUIRE(re_match_n("a", 1u, no_match, sizeof(no_match) - 1u,
                       &limits, &result) == RE_STATUS_TEXT_LIMIT);
    limits = re_limits_default();
    limits.max_pattern_bytes = 2u;
    REQUIRE(re_validate_n("a+b", 3u, &limits) == RE_STATUS_PATTERN_LIMIT);

    REQUIRE(re_match_n("q", 1u, no_match, sizeof(no_match) - 1u,
                       NULL, &result) == RE_STATUS_NO_MATCH);
    REQUIRE(re_compile("*bad") == NULL);
    REQUIRE(re_compile_n(NULL, 1u, NULL, &compiled)
            == RE_STATUS_INVALID_ARGUMENT);
    REQUIRE(compiled == NULL);
    puts("PASS original regex C11 parser, original bounded limits, binary matching and owner retirement");
    return 0;
}
