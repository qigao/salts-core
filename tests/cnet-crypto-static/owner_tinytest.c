/* Canonical TinyTest runner for the optional real CNet static Crypto owner.
 * The original main remains unmodified as a test-only legacy entry.
 * Python TLS/mTLS peer invocations must keep their exact stdout protocol
 * and original exit semantics; they are external peer drivers, not unit tests.
 */
#define TINYTEST_NO_MAIN
#include <tinytest.h>

int saltscore_cnet_crypto_legacy_main(int argc, char **argv);

static int owner_argc;
static char **owner_argv;

spec("Original CNet single static Crypto owner") {
  it("qualifies real TLS 1.2/1.3, strict CA/SAN and owner admission") {
    const int status = saltscore_cnet_crypto_legacy_main(owner_argc, owner_argv);
    check(status == 0, "CNet static owner native integration returned %d", status);
  }
}

int main(int argc, char **argv) {
  if (!argv || argc < 1 || !argv[0]) return 2;
  if (argc != 1) {
    /* --mtls / --external-san are observed by independent Python peers.
     * Do not interleave TinyTest's discovery/summary text with peer output. */
    return saltscore_cnet_crypto_legacy_main(argc, argv);
  }
  owner_argc = argc;
  owner_argv = argv;
  char *runner_argv[] = {argv[0]};
  return ttest_main__(1, runner_argv, TTEST_INVOKE_SPEC_ADAPTER__,
                      TT_USE_COLOR != 0, TT_USE_TAP != 0,
                      TTEST_IS_ATTY__() != 0, TTEST_PRINT_TRACE_DEFAULT__);
}
