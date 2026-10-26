#include <salts_core/runtime_abi.h>
#include <cmeta_buffer.h>
int main(void) {
  salts_core_runtime_abi_0_1_required();
  if (salts_core_runtime_abi_version() != SALTS_CORE_RUNTIME_ABI_VERSION ||
      SALTS_CORE_RUNTIME_ABI_MAJOR != 0u ||
      SALTS_CORE_RUNTIME_ABI_MINOR != 1u ||
      salts_core_runtime_mem_pool_size() != sizeof(mem_pool_t) ||
      salts_core_runtime_mem_buffer_size() != sizeof(mem_buffer_t) ||
      salts_core_runtime_mem_slice_size() != sizeof(mem_slice_t)) return 1;
  return 0;
}
