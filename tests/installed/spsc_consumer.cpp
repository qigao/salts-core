#include <ring_buffer_spsc.h>
#include <bucket_priority_queue_spsc.h>
#include <type_traits>
#include <cstdint>
static_assert(std::is_standard_layout<ring_spsc_t>::value,
              "Native single-producer ring must be standard layout");
int main() {
  std::uint8_t bytes[16] = {0};
  ring_spsc_t ring {};
  if (!ring_spsc_init(&ring,bytes,sizeof(bytes))) return 1;
  auto *p=ring_spsc_write_acquire(&ring,2);
  if(!p) return 2;
  p[0]='o';p[1]='k';ring_spsc_write_release(&ring,2);
  std::size_t size=0;
  p=ring_spsc_read_acquire(&ring,&size);
  if(!p || size!=2 || p[0]!='o' || p[1]!='k') return 3;
  ring_spsc_read_release(&ring,2);
  return ring_spsc_read_available(&ring)==0 ? 0 : 4;
}
