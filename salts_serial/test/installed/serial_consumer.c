#include <salts_serial.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <type_traits>
#define SIGNATURE(name, ...) static_assert(std::is_same_v<decltype(&name), __VA_ARGS__>, #name)
#else
#define SIGNATURE(name, ...) _Static_assert(_Generic(&name, __VA_ARGS__: 1, default: 0), #name)
#endif

#define REQUIRE(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "Installed Serial contract failed at line %d: %s\n", \
            __LINE__, #condition); \
    return EXIT_FAILURE; \
  } \
} while (0)

SIGNATURE(salts_serial_config_default, void (*)(salts_serial_config_t *));
SIGNATURE(salts_serial_result_name, const char *(*)(salts_serial_result_t));
SIGNATURE(salts_serial_list_ports, salts_serial_result_t (*)(salts_serial_port_list_t **));
SIGNATURE(salts_serial_port_info_by_name, salts_serial_result_t (*)(const char *, salts_serial_port_list_t **, const salts_serial_port_info_t **));
SIGNATURE(salts_serial_port_list_destroy, void (*)(salts_serial_port_list_t *));
SIGNATURE(salts_serial_port_list_count, size_t (*)(const salts_serial_port_list_t *));
SIGNATURE(salts_serial_port_list_get, const salts_serial_port_info_t *(*)(const salts_serial_port_list_t *, size_t));
SIGNATURE(salts_serial_create, salts_serial_result_t (*)(salts_serial_t **, const salts_serial_config_t *));
SIGNATURE(salts_serial_destroy, void (*)(salts_serial_t *));
SIGNATURE(salts_serial_open, salts_serial_result_t (*)(salts_serial_t *, const char *, salts_serial_mode_t));
SIGNATURE(salts_serial_close, salts_serial_result_t (*)(salts_serial_t *));
SIGNATURE(salts_serial_read, salts_serial_result_t (*)(salts_serial_t *, void *, size_t, unsigned int, size_t *));
SIGNATURE(salts_serial_write, salts_serial_result_t (*)(salts_serial_t *, const void *, size_t, unsigned int, size_t *));
SIGNATURE(salts_serial_start_async, salts_serial_result_t (*)(salts_serial_t *));
SIGNATURE(salts_serial_stop_async, salts_serial_result_t (*)(salts_serial_t *));
SIGNATURE(salts_serial_async_running, int (*)(const salts_serial_t *));
SIGNATURE(salts_serial_last_error, salts_serial_result_t (*)(const salts_serial_t *));
SIGNATURE(salts_serial_event_set_create, salts_serial_result_t (*)(salts_serial_event_set_t **));
SIGNATURE(salts_serial_event_set_destroy, void (*)(salts_serial_event_set_t *));
SIGNATURE(salts_serial_event_set_add, salts_serial_result_t (*)(salts_serial_event_set_t *, const salts_serial_t *, unsigned int));
SIGNATURE(salts_serial_event_wait, salts_serial_result_t (*)(salts_serial_event_set_t *, unsigned int));
SIGNATURE(salts_serial_event_wait_ex, salts_serial_result_t (*)(salts_serial_event_set_t *, unsigned int, unsigned int *));
SIGNATURE(salts_serial_rx_available, size_t (*)(const salts_serial_t *));
SIGNATURE(salts_serial_tx_available, size_t (*)(const salts_serial_t *));
SIGNATURE(salts_serial_rx_capacity, size_t (*)(const salts_serial_t *));
SIGNATURE(salts_serial_tx_capacity, size_t (*)(const salts_serial_t *));
SIGNATURE(salts_serial_buffer_rx, salts_serial_result_t (*)(salts_serial_t *, const void *, size_t, size_t *));
SIGNATURE(salts_serial_read_buffered, salts_serial_result_t (*)(salts_serial_t *, void *, size_t, size_t *));
SIGNATURE(salts_serial_write_buffered, salts_serial_result_t (*)(salts_serial_t *, const void *, size_t, size_t *));
SIGNATURE(salts_serial_drain_tx_buffer, salts_serial_result_t (*)(salts_serial_t *, void *, size_t, size_t *));

int main(void) {
  const unsigned char first[] = {0u, 'A', 0x80u, 0xffu, 0xc3u, 0xa9u, 'Z', 0x7fu};
  const unsigned char tail[] = {0x7fu, 0u, 0xfeu};
  const unsigned char expected[] = {0xffu, 0xc3u, 0xa9u, 'Z', 0x7fu, 0u, 0xfeu};
  unsigned char received[16] = {0};
  salts_serial_t *serial = NULL;
  salts_serial_config_t config;
  salts_serial_port_list_t *ports = NULL;
  const salts_serial_port_info_t *info = NULL;
  salts_serial_event_set_t *events = NULL;
  char metadata_name[] = "native-\xc3\xa9";
  size_t count = 99u;
  unsigned int mask = 99u;

  /* Every original function is called, so missing exports fail real linkage. */
  salts_serial_config_default(&config);
  REQUIRE(config.baudrate == 115200 && config.bits == 8);
  REQUIRE(strcmp(salts_serial_result_name(SALTS_SERIAL_WOULD_BLOCK), "would block") == 0);
  REQUIRE(salts_serial_create(NULL, &config) == SALTS_SERIAL_INVALID_VALUE);
  REQUIRE(salts_serial_list_ports(&ports) == SALTS_SERIAL_OK);
  REQUIRE(ports != NULL);
  REQUIRE(salts_serial_port_list_get(ports, salts_serial_port_list_count(ports)) == NULL);
  salts_serial_port_list_destroy(ports);
  ports = NULL;
  REQUIRE(salts_serial_port_info_by_name(metadata_name, &ports, &info) == SALTS_SERIAL_OK);
  REQUIRE(ports != NULL && info != NULL && info->name != NULL);
  REQUIRE(salts_serial_port_list_count(ports) == 1u);
  metadata_name[0] = 'X';
  REQUIRE(strcmp(info->name, "native-\xc3\xa9") == 0);
  salts_serial_port_list_destroy(ports);
  ports = NULL;

  config.rx_buffer_size = 8u;
  config.tx_buffer_size = 8u;
  REQUIRE(salts_serial_create(&serial, &config) == SALTS_SERIAL_OK);
  REQUIRE(serial != NULL);
  REQUIRE(salts_serial_rx_capacity(serial) == 7u);
  REQUIRE(salts_serial_tx_capacity(serial) == 7u);
  REQUIRE(salts_serial_rx_available(serial) == 0u);
  REQUIRE(salts_serial_tx_available(serial) == 0u);
  REQUIRE(salts_serial_open(serial, "not-a-real-port", (salts_serial_mode_t)0) == SALTS_SERIAL_INVALID_VALUE);
  REQUIRE(salts_serial_read(serial, received, sizeof(received), 1u, &count) == SALTS_SERIAL_INVALID_STATE);
  REQUIRE(count == 0u);
  REQUIRE(salts_serial_write(serial, first, sizeof(first), 1u, &count) == SALTS_SERIAL_INVALID_STATE);
  REQUIRE(count == 0u);
  REQUIRE(salts_serial_start_async(serial) == SALTS_SERIAL_INVALID_STATE);
  REQUIRE(salts_serial_async_running(serial) == 0);
  REQUIRE(salts_serial_stop_async(serial) == SALTS_SERIAL_OK);
  REQUIRE(salts_serial_last_error(serial) == SALTS_SERIAL_OK);

  REQUIRE(salts_serial_buffer_rx(serial, first, sizeof(first), &count) == SALTS_SERIAL_OK);
  REQUIRE(count == 7u);
  REQUIRE(salts_serial_buffer_rx(serial, first, 1u, &count) == SALTS_SERIAL_WOULD_BLOCK);
  REQUIRE(count == 0u);
  REQUIRE(salts_serial_read_buffered(serial, received, 3u, &count) == SALTS_SERIAL_OK);
  REQUIRE(count == 3u && memcmp(received, first, 3u) == 0);
  REQUIRE(salts_serial_buffer_rx(serial, tail, sizeof(tail), &count) == SALTS_SERIAL_OK);
  REQUIRE(count == sizeof(tail));
  REQUIRE(salts_serial_read_buffered(serial, received, sizeof(received), &count) == SALTS_SERIAL_OK);
  REQUIRE(count == sizeof(expected));
  REQUIRE(memcmp(received, expected, sizeof(expected)) == 0);
  REQUIRE(salts_serial_read_buffered(serial, received, sizeof(received), &count) == SALTS_SERIAL_WOULD_BLOCK);
  REQUIRE(count == 0u);
  REQUIRE(salts_serial_write_buffered(serial, expected, sizeof(expected), &count) == SALTS_SERIAL_OK);
  REQUIRE(count == sizeof(expected));
  REQUIRE(salts_serial_tx_available(serial) == sizeof(expected));
  REQUIRE(salts_serial_drain_tx_buffer(serial, received, sizeof(received), &count) == SALTS_SERIAL_OK);
  REQUIRE(count == sizeof(expected) && memcmp(received, expected, sizeof(expected)) == 0);

  REQUIRE(salts_serial_event_set_create(&events) == SALTS_SERIAL_OK);
  REQUIRE(salts_serial_event_set_add(events, serial, SALTS_SERIAL_EVENT_RX_READY) == SALTS_SERIAL_INVALID_STATE);
  REQUIRE(salts_serial_event_wait(events, 1u) == SALTS_SERIAL_INVALID_STATE);
  REQUIRE(salts_serial_event_wait_ex(events, 1u, &mask) == SALTS_SERIAL_INVALID_STATE);
  REQUIRE(mask == 0u);
  salts_serial_event_set_destroy(events);
  REQUIRE(salts_serial_close(serial) == SALTS_SERIAL_OK);
  salts_serial_destroy(serial);
  return EXIT_SUCCESS;
}
