/*
 * Clean-prefix C11 SDK consumer and a separately linked loadable module must
 * share exactly one pool and same-thread TLS identity while loading CNet.
 */
#include <cnet/cnet.h>
#include <cmeta_buffer.h>
#include <salts/thread.h>
#include <salts/error_codes.h>
#include <stdio.h>

extern mem_pool_t *saltscore_owner_probe_pool(void);
extern const void *saltscore_owner_probe_thread(void);
extern void saltscore_owner_probe_release(mem_slice_t *slice);
extern mem_slice_t saltscore_owner_probe_make_slice(void);

static cnet_client_config config_for_os(void) {
  cnet_client_config cfg = {0};
#if defined(_WIN32)
  cfg.backend = NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
  cfg.backend = NATIVE_IO_BACKEND_KQUEUE;
#else
  cfg.backend = NATIVE_IO_BACKEND_EPOLL;
#endif
  cfg.connection_capacity = 2u;
  cfg.command_capacity = 8u;
  cfg.request_capacity = 8u;
  cfg.completion_batch_capacity = 8u;
  cfg.event_capacity = 8u;
  cfg.max_send_bytes = 1024u;
  cfg.receive_buffer_bytes = 1024u;
  return cfg;
}

int main(void) {
  cnet_client client = {0};
  cnet_client_config config = config_for_os();
  mem_buffer_t *buffer = NULL;
  mem_slice_t consumer_slice = {0};
  mem_slice_t module_slice = {0};
  int live = 0;
  int result = 0;

#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "SDK ABI owner failure line %d: %s\\n", __LINE__, #expr); \
    result = __LINE__; goto done; \
  } } while (0)

  REQUIRE(saltscore_owner_probe_pool() == mem_global());
  REQUIRE(saltscore_owner_probe_thread() == cmeta_thread_current_token());

  /* Actually reference CNet to make --as-needed and Windows link load it. */
  REQUIRE(cnet_client_init(&client, &config) == SALTS_OK);
  live = 1;

  buffer = mem_get_buffer(mem_global(), 1u);
  REQUIRE(buffer != NULL);
  mem_buffer_data(buffer)[0] = (char)0xa5u;
  mem_set_used(buffer, 1u);
  consumer_slice = mem_slice(buffer, 0u, 1u);
  REQUIRE(consumer_slice.buffer != NULL);
  mem_buffer_release(buffer);
  buffer = NULL;
  REQUIRE(mem_buffer_pool(consumer_slice.buffer) == saltscore_owner_probe_pool());
  REQUIRE((unsigned char)consumer_slice.data[0] == 0xa5u);

  /* Consumer allocated, plugin releases; plugin allocated, consumer releases. */
  saltscore_owner_probe_release(&consumer_slice);
  REQUIRE(consumer_slice.buffer == NULL);
  module_slice = saltscore_owner_probe_make_slice();
  REQUIRE(module_slice.buffer != NULL && module_slice.length == 1u);
  REQUIRE(mem_buffer_pool(module_slice.buffer) == mem_global());
  REQUIRE((unsigned char)module_slice.data[0] == 0x5au);

  REQUIRE(cnet_client_stop(&client, 2000u) == SALTS_OK);
  REQUIRE(cnet_client_destroy(&client) == SALTS_OK);
  live = 0;
  REQUIRE((unsigned char)module_slice.data[0] == 0x5au);
  REQUIRE(saltscore_owner_probe_pool() == mem_global());
  REQUIRE(saltscore_owner_probe_thread() == cmeta_thread_current_token());

done:
  if (buffer) mem_buffer_release(buffer);
  if (consumer_slice.buffer) saltscore_owner_probe_release(&consumer_slice);
  if (module_slice.buffer) mem_slice_release(&module_slice);
  if (live) {
    (void)cnet_client_stop(&client, 2000u);
    (void)cnet_client_destroy(&client);
  }
  return result;
}
