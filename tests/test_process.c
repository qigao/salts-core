#include <salts/process.h>
#include <salts/error_codes.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  salts_process_options_t opts;
  salts_process_result_t result;
  salts_process_t *child = NULL;
  const char *const args[] = {"--core-child", NULL};
  char bytes[128] = {0};
  size_t count = 0;
  if (argc > 1 && strcmp(argv[1], "--core-child") == 0) {
    fputs("SaltsProcessChild:ok\n", stdout);
    fflush(stdout);
    return 19;
  }
  if (!argv || !argv[0] || !argv[0][0]) return 1;
  salts_process_options_init(&opts);
  opts.program = argv[0];
  opts.args = args;
  opts.flags = SALTS_PROCESS_CAPTURE_STDOUT | SALTS_PROCESS_CAPTURE_STDERR;
  opts.timeout_ms = 10000;
  if (salts_process_spawn(&opts, &child) != SALTS_OK || !child) return 2;
  if (salts_process_wait_for(child, 12000, &result) != SALTS_OK) return 3;
  if (result.state != SALTS_PROCESS_EXITED || result.exit_code != 19) return 4;
  if (salts_process_read_stdout(child, bytes, sizeof(bytes), &count) != SALTS_OK)
    return 5;
  if (count < sizeof("SaltsProcessChild:ok") - 1u ||
      memcmp(bytes, "SaltsProcessChild:ok", sizeof("SaltsProcessChild:ok") - 1u) != 0)
    return 6;
  salts_process_destroy(child);
  child = NULL;
  if (salts_process_spawn(NULL, &child) == SALTS_OK || child) return 7;
  puts("Salts::Process real child, exit and retained capture PASS");
  return 0;
}
