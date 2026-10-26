#include "websocket_frame_parser.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)

static int control_and_borrow(void) {
  static const uint8_t simple[] = {0x81, 0x02, 'h', 'i'};
  uint8_t masked[] = {0x82, 0x82, 1, 2, 3, 4, (uint8_t)('h' ^ 1), (uint8_t)('i' ^ 2)};
  ws_frame_t frame = {0};
  size_t needed = 771;

  CHECK(cmeta_wsparser_frame_peek_size(simple, 1, &needed) == WS_PARSE_NEED_MORE);
  CHECK(needed == 771);
  CHECK(cmeta_wsparser_frame_peek_size(simple, 2, &needed) == WS_PARSE_OK);
  CHECK(needed == sizeof(simple));
  CHECK(cmeta_wsparser_frame_parse(simple, 3, &frame) == WS_PARSE_NEED_MORE);
  CHECK(frame.payload == NULL && frame.opcode == 0);
  CHECK(cmeta_wsparser_frame_parse(simple, sizeof(simple), &frame) == WS_PARSE_OK);
  CHECK(frame.fin && !frame.masked && frame.opcode == WS_OPCODE_TEXT);
  CHECK(frame.header_len == 2 && frame.payload_len == 2);
  CHECK(frame.payload == simple + 2 && memcmp(frame.payload, "hi", 2) == 0);

  frame = (ws_frame_t){0};
  CHECK(cmeta_wsparser_frame_parse(masked, sizeof(masked), &frame) == WS_PARSE_OK);
  CHECK(frame.masked && frame.opcode == WS_OPCODE_BINARY);
  CHECK(frame.header_len == 6 && frame.payload == masked + 6);
  CHECK(memcmp(frame.masking_key, (uint8_t[]){1, 2, 3, 4}, 4) == 0);
  CHECK(cmeta_wsparser_frame_unmask(masked + 6, 2, frame.masking_key) == WS_PARSE_OK);
  CHECK(memcmp(masked + 6, "hi", 2) == 0);
  CHECK(cmeta_wsparser_frame_unmask(NULL, 0, NULL) == WS_PARSE_OK);
  CHECK(cmeta_wsparser_frame_unmask(NULL, 1, frame.masking_key) == WS_PARSE_INVALID_ARGUMENT);
  CHECK(cmeta_wsparser_frame_unmask(masked, 1, NULL) == WS_PARSE_INVALID_ARGUMENT);
  return 0;
}

static int invalid_frames_preserve_output(void) {
  static const uint8_t invalid_opcode[] = {0x83, 0};
  static const uint8_t invalid_rsv[] = {0xC1, 0};
  static const uint8_t fragmented_control[] = {0x09, 0};
  static const uint8_t oversized_control[] = {0x89, 126, 0, 126};
  static const uint8_t noncanonical_16[] = {0x82, 126, 0, 125};
  static const uint8_t noncanonical_64[] = {0x82, 127, 0, 0, 0, 0, 0, 0, 0, 126};
  static const uint8_t invalid_64[] = {0x82, 127, 0x80, 0, 0, 0, 0, 0, 0, 0};
  const struct { const uint8_t *ptr; size_t n; ws_parse_result_t expected; } cases[] = {
    {invalid_opcode, sizeof invalid_opcode, WS_PARSE_INVALID_OPCODE},
    {invalid_rsv, sizeof invalid_rsv, WS_PARSE_INVALID_RSV},
    {fragmented_control, sizeof fragmented_control, WS_PARSE_FRAGMENTED_CONTROL},
    {oversized_control, sizeof oversized_control, WS_PARSE_CONTROL_TOO_LARGE},
    {noncanonical_16, sizeof noncanonical_16, WS_PARSE_NON_CANONICAL_LENGTH},
    {noncanonical_64, sizeof noncanonical_64, WS_PARSE_NON_CANONICAL_LENGTH},
    {invalid_64, sizeof invalid_64, WS_PARSE_INVALID_LENGTH}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
    ws_frame_t frame = {0};
    frame.opcode = 0x55;
    frame.payload_len = 0x1234;
    CHECK(cmeta_wsparser_frame_parse(cases[i].ptr, cases[i].n, &frame) == cases[i].expected);
    CHECK(frame.opcode == 0x55 && frame.payload_len == 0x1234);
  }
  {
    const uint8_t tiny[] = {0x82, 127, 0};
    ws_frame_t frame = {0};
    frame.opcode = 0x55;
    CHECK(cmeta_wsparser_frame_parse(tiny, sizeof tiny, &frame) == WS_PARSE_NEED_MORE);
    CHECK(frame.opcode == 0x55);
  }
  CHECK(cmeta_wsparser_frame_parse(NULL, 0, NULL) == WS_PARSE_INVALID_ARGUMENT);
  CHECK(cmeta_wsparser_frame_peek_size(NULL, 0, NULL) == WS_PARSE_INVALID_ARGUMENT);
  return 0;
}

static int canonical_header_contract(void) {
  const uint64_t sizes[] = {0, 1, 125, 126, 127, 65535, 65536, 70000};
  const uint8_t key[4] = {0x10, 0x20, 0x30, 0x40};
  for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i) {
    for (int masked = 0; masked <= 1; ++masked) {
      uint8_t buffer[WS_FRAME_MAX_HEADER_BYTES] = {0};
      size_t written = SIZE_MAX;
      size_t needed = SIZE_MAX;
      const size_t expected_len = cmeta_wsparser_frame_header_len(sizes[i], masked);
      ws_frame_t frame = {0};
      CHECK(expected_len <= sizeof buffer);
      CHECK(cmeta_wsparser_frame_build_header(buffer, sizeof buffer, WS_OPCODE_BINARY,
             sizes[i], 1, masked, masked ? key : NULL, &written) == WS_PARSE_OK);
      CHECK(written == expected_len);
      CHECK(buffer[0] == 0x82 && ((buffer[1] >> 7) == (unsigned)masked));
      CHECK(cmeta_wsparser_frame_peek_size(buffer, written, &needed) == WS_PARSE_OK);
      CHECK(needed == written + (size_t)sizes[i]);
      CHECK(cmeta_wsparser_frame_parse(buffer, written, &frame) ==
             (sizes[i] == 0 ? WS_PARSE_OK : WS_PARSE_NEED_MORE));
      if (sizes[i] == 0)
        CHECK(frame.opcode == WS_OPCODE_BINARY && frame.payload_len == 0 &&
              frame.header_len == expected_len);
    }
  }
  return 0;
}

static int builder_rejects_without_modification(void) {
  uint8_t buffer[WS_FRAME_MAX_HEADER_BYTES];
  size_t written;
  memset(buffer, 0xa5, sizeof buffer);
  written = 777;
  CHECK(cmeta_wsparser_frame_build_header(buffer, sizeof buffer, WS_OPCODE_TEXT,
        UINT64_MAX, 1, 0, NULL, &written) == WS_PARSE_INVALID_LENGTH);
  CHECK(written == 777 && buffer[0] == 0xa5);
  CHECK(cmeta_wsparser_frame_build_header(buffer, 1, WS_OPCODE_TEXT,
        0, 1, 0, NULL, &written) == WS_PARSE_NEED_MORE);
  CHECK(written == 777 && buffer[0] == 0xa5);
  CHECK(cmeta_wsparser_frame_build_header(buffer, sizeof buffer, WS_OPCODE_PING,
        126, 1, 0, NULL, &written) == WS_PARSE_CONTROL_TOO_LARGE);
  CHECK(written == 777 && buffer[0] == 0xa5);
  CHECK(cmeta_wsparser_frame_build_header(buffer, sizeof buffer, WS_OPCODE_PING,
        0, 0, 0, NULL, &written) == WS_PARSE_FRAGMENTED_CONTROL);
  CHECK(written == 777 && buffer[0] == 0xa5);
  CHECK(cmeta_wsparser_frame_build_header(buffer, sizeof buffer, WS_OPCODE_TEXT,
        1, 1, 1, NULL, &written) == WS_PARSE_INVALID_ARGUMENT);
  CHECK(written == 777 && buffer[0] == 0xa5);
  return 0;
}

int main(void) {
  int result = control_and_borrow();
  if (result) return result;
  result = invalid_frames_preserve_output();
  if (result) return result;
  result = canonical_header_contract();
  if (result) return result;
  return builder_rejects_without_modification();
}
