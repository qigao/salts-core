# Original qigao/salts utils/verification models live unchanged in this
# standalone source-owned module. These ARE original Spin inputs, not stubs.
# No custom target or CTest execution here claims formal model acceptance.
set(_original_salts_model_sources
  buffer_ownership.pml
  byte_buffer.pml
  byte_buffer_errors.pml
  disruptor_broadcast.pml
  disruptor_topology.pml
  disruptor_wait_shutdown.pml
  disruptor_worker.pml
  priority_queue.pml
  priority_queue_timeout.pml
  ring_spsc.pml
)
foreach(_model IN LISTS _original_salts_model_sources)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_model}")
    message(FATAL_ERROR "Missing original Promela model: ${_model}")
  endif()
endforeach()
add_custom_target(saltscore_original_promela_models SOURCES
  ${_original_salts_model_sources} README.md MIGRATION.md)
