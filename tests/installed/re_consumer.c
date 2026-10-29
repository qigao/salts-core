/* Independent installed C11 consumer: all regex symbols from Salts::Core. */
#include "re.h"
#include <stddef.h>
#include <stdio.h>

int main(void)
{
    re_t p = NULL;
    re_match_result_t match = {RE_NPOS, 0u};
    int len = 0;
    if (re_compile_n("a{2,4}", 6u, NULL, &p) != RE_STATUS_OK || !p)
        return 1;
    if (re_matchn(p, "xaaaz", 5u, NULL, &match) != RE_STATUS_OK) {
        re_destroy(p);
        return 2;
    }
    re_destroy(p);
    if (match.index != 1u || match.length != 3u) return 3;
    if (re_match("foo|bar", "xxbar", &len) != 2 || len != 3) return 4;
    puts("PASS installed C11 regex originally-owned Core ABI");
    return 0;
}
