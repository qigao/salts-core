#include <salts/process.h>

static void unused_release(void *) {}

int main() {
  salts_process_options_t opts{};
  salts_process_retainer_t owner{};
  salts_process_retainer_token_t token{};
  salts_process_t *process = nullptr;
  size_t reaped = 0;
  size_t pending = 0;
  int payload = 0;

  salts_process_options_init(&opts);
  if (opts.max_output_bytes == 0) return 1;
  if (salts_process_retainer_init(&owner, 1) != SALTS_OK) return 2;
  if (salts_process_retainer_reserve(&owner, &token) != SALTS_OK) return 3;
  if (salts_process_retainer_adopt(
          &owner, token, &process, &payload, unused_release) != SALTS_EINVAL)
    return 4;
  if (salts_process_retainer_run_ready(&owner, 1, &reaped, &pending) !=
          SALTS_EBUSY ||
      reaped != 0 || pending != 1)
    return 5;
  if (salts_process_retainer_release(&owner, token) != SALTS_OK) return 6;
  if (salts_process_retainer_destroy(&owner) != SALTS_OK || owner.impl != nullptr)
    return 7;
  return 0;
}
