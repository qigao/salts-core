#include <cnet/cnet.h>
#include <cnet/destination_policy.h>
#include <salts/error_codes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
  cnet_client client = {0};
  cnet_client_config config = {0};
  const cnet_destination_hint hints[1] = {{7u, 1u, 0u, true}};
  cnet_destination_selection selection = {0};
  cnet_destination_result result = {0};
  int status;

#if defined(_WIN32)
  config.backend = NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
  config.backend = NATIVE_IO_BACKEND_KQUEUE;
#else
  config.backend = NATIVE_IO_BACKEND_EPOLL;
#endif
  config.connection_capacity = 2u;
  config.command_capacity = 8u;
  config.request_capacity = 8u;
  config.completion_batch_capacity = 8u;
  config.event_capacity = 8u;
  config.max_send_bytes = 1024u;
  config.receive_buffer_bytes = 1024u;

  selection.size = sizeof selection;
  selection.version = CNET_DESTINATION_POLICY_VERSION;
  selection.kind = CNET_DESTINATION_EXPLICIT;
  selection.endpoints = hints;
  selection.endpoint_count = 1u;
  selection.snapshot_generation = 1u;
  selection.expires_at_ms = UINT64_MAX;
  selection.explicit_endpoint_id = 7u;
  if (cnet_destination_choose(&selection, &result) != SALTS_OK ||
      result.endpoint_id != 7u) return 1;

  status = cnet_client_init(&client, &config);
  if (status != SALTS_OK) {
    fprintf(stderr, "cnet_client_init failed: %d\n", status);
    return 2;
  }
  if (cnet_client_stop(&client, 2000u) != SALTS_OK) return 3;
  if (cnet_client_destroy(&client) != SALTS_OK) return 4;
  return 0;
}
