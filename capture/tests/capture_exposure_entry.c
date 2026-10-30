#include <tinytest.h>
#include <stdio.h>

/* The frozen Windows capture donor owns the test cases. This tiny entrypoint
 * only exposes startup/runtime diagnostics; it does not change the assertions. */
int main(int argc, char **argv) {
    fputs("capture fake-COM: entered main\n", stderr);
    fflush(stderr);
    int result = ttest_main__(argc, argv, NULL, TT_USE_COLOR != 0,
                              TT_USE_TAP != 0, TTEST_IS_ATTY__() != 0,
                              TTEST_PRINT_TRACE_DEFAULT__);
    fprintf(stderr, "capture fake-COM: tinytest exit=%d\n", result);
    fflush(stderr);
    return result;
}
