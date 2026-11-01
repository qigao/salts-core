#include <ring_buffer_spsc.h>
#include <bucket_priority_queue_spsc.h>
#include <stdint.h>
#include <string.h>
int main(void) {
  uint8_t buffer[32] = {0};
  ring_spsc_t ring = {0};
  if (!ring_spsc_init(&ring,buffer,sizeof(buffer))) return 1;
  uint8_t *p = ring_spsc_write_acquire(&ring,4);
  if (!p) return 2;
  memcpy(p,"test",4); ring_spsc_write_release(&ring,4);
  size_t n = 0; p = ring_spsc_read_acquire(&ring,&n);
  if (!p || n != 4 || memcmp(p,"test",4)) return 3;
  ring_spsc_read_release(&ring,4);
  if (ring_spsc_read_available(&ring)) return 4;
  bucket_priority_queue_spsc_t q = {0};
  if (!bucket_priority_queue_spsc_init(&q,16)) return 5;
  if (!bucket_priority_queue_spsc_push(&q,BUCKET_PRIORITY_SPSC_LOW,11) ||
      !bucket_priority_queue_spsc_push(&q,BUCKET_PRIORITY_SPSC_HIGH,21)) {
     bucket_priority_queue_spsc_destroy(&q); return 6;
  }
  bucket_priority_spsc_value_t value = 0;
  if (!bucket_priority_queue_spsc_pop(&q,&value) || value != 21) {
    bucket_priority_queue_spsc_destroy(&q); return 7;
  }
  if (!bucket_priority_queue_spsc_pop(&q,&value) || value != 11) {
    bucket_priority_queue_spsc_destroy(&q); return 8;
  }
  bucket_priority_queue_spsc_destroy(&q);
  return 0;
}
