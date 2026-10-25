#include <ring_buffer.h>
#include <stdint.h>
#include <string.h>

int main(void) {
  uint8_t data[16] = {0};
  ring_data_type queue = {0};
  uint8_t *ptr;
  size_t available = 999u;
  ring_init(&queue, data, sizeof(data));
  if (ring_read_acquire(&queue, &available) != NULL || available != 0u)
    return 1;
  ptr = ring_write_acquire(&queue, 10u);
  if (ptr == NULL) return 2;
  memcpy(ptr, "abcdefghij", 10u);
  ring_write_release(&queue, 10u);
  ptr = ring_read_acquire(&queue, &available);
  if (ptr == NULL || available != 10u || memcmp(ptr, "abcdefghij", 10u) != 0)
    return 3;
  ring_read_release(&queue, available);

  /* A full read preserves the write tail at 10; the next 8-byte write
   * must wrap to 0 without leaking invalidated tail bytes. */
  ptr = ring_write_acquire(&queue, 8u);
  if (ptr != data) return 4;
  memcpy(ptr, "12345678", 8u);
  ring_write_release(&queue, 8u);
  ptr = ring_read_acquire(&queue, &available);
  if (ptr != data || available != 8u || memcmp(ptr, "12345678", 8u) != 0)
    return 5;
  ring_read_release(&queue, 8u);
  if (ring_read_acquire(&queue, &available) != NULL || available != 0u)
    return 6;
  if (ring_write_acquire(&queue, sizeof(data)) != NULL) return 7;
  return 0;
}
