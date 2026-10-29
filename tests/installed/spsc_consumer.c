#include <ring_buffer_spsc.h>
#include <bucket_priority_queue_spsc.h>
#include <string.h>
int main(void) {
  uint8_t bytes[16] = {0};
  ring_spsc_t ring = {0};
  if (!ring_spsc_init(&ring, bytes, sizeof(bytes))) return 1;
  uint8_t *p = ring_spsc_write_acquire(&ring,5u);
  if (!p) return 2;
  memcpy(p,"hello",5u); ring_spsc_write_release(&ring,5u);
  size_t n=0; p=ring_spsc_read_acquire(&ring,&n);
  if (!p || n!=5u || memcmp(p,"hello",5u)!=0) return 3;
  ring_spsc_read_release(&ring,5u);
  bucket_priority_queue_spsc_t q={0};
  if (!bucket_priority_queue_spsc_init(&q,16u)) return 4;
  if (!bucket_priority_queue_spsc_push(&q,BUCKET_PRIORITY_SPSC_CRITICAL,47u)) {
    bucket_priority_queue_spsc_destroy(&q);return 5;
  }
  bucket_priority_spsc_value_t value=0;
  int ok=bucket_priority_queue_spsc_pop(&q,&value) && value==47u;
  bucket_priority_queue_spsc_destroy(&q);
  return ok?0:6;
}
