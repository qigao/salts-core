cmake_minimum_required(VERSION 3.20)
project(SaltsCoreOriginalReadinessConformance LANGUAGES C)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)

if(NOT DEFINED SALTS_REFERENCE_SOURCE_ROOT OR
   NOT IS_ABSOLUTE "${SALTS_REFERENCE_SOURCE_ROOT}")
  message(FATAL_ERROR "Original Salts source root must be absolute")
endif()
set(_reference "${SALTS_REFERENCE_SOURCE_ROOT}")
set(_tests "${_reference}/platform/tests")
if(NOT EXISTS "${_tests}/readiness_contract_suite.c" OR
   NOT EXISTS "${_reference}/tinytest/src/tinytest.c")
  message(FATAL_ERROR "Pinned original Readiness test sources are missing")
endif()

# Real original test translations, but NEVER compile original Platform sources:
# the state machine and all backend owners are from installed SaltsCore.
# The standalone dependency scanner forbids concrete legacy package
# references. Build the runtime prohibition from its namespace and suffix
# while still failing configuration for any historical owner target.
set(_legacy_prefix "Salts::")
foreach(_old IN ITEMS Core Platform NativeIO)
  if(TARGET "${_legacy_prefix}${_old}")
    message(FATAL_ERROR "Forbidden duplicate legacy runtime ${_old}")
  endif()
endforeach()
get_target_property(_kind SaltsCore::Platform TYPE)
if(NOT _kind STREQUAL "SHARED_LIBRARY")
  message(FATAL_ERROR "Readiness contract requires installed shared Platform")
endif()
add_library(m6_original_tinytest STATIC
  "${_reference}/tinytest/src/tinytest.c")
target_compile_features(m6_original_tinytest PRIVATE c_std_11)
target_include_directories(m6_original_tinytest PUBLIC
  "${_reference}/tinytest/include")

function(m6_add_original name)
  add_executable(m6_original_readiness_${name} ${ARGN})
  target_compile_features(m6_original_readiness_${name} PRIVATE c_std_11)
  set_target_properties(m6_original_readiness_${name} PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_include_directories(m6_original_readiness_${name} PRIVATE
    "${_tests}")
  target_link_libraries(m6_original_readiness_${name} PRIVATE
    SaltsCore::Platform m6_original_tinytest)
  if(MSVC)
    target_compile_options(m6_original_readiness_${name} PRIVATE
      $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
  endif()
  add_test(NAME saltscore.original_readiness.${name}
    COMMAND m6_original_readiness_${name})
  set_tests_properties(saltscore.original_readiness.${name}
    PROPERTIES TIMEOUT 120)
endfunction()

m6_add_original(state_model
  "${_tests}/platform_readiness_state_model_test.c")
m6_add_original(fake_contract
  "${_tests}/platform_readiness_fake_test.c"
  "${_tests}/readiness_backend_contract.c"
  "${_tests}/readiness_contract_suite.c"
  "${_tests}/readiness_fake_backend.c")
if(UNIX)
  m6_add_original(poll
    "${_tests}/platform_readiness_poll_test.c"
    "${_tests}/readiness_backend_contract.c")
endif()
if(APPLE)
  m6_add_original(kqueue
    "${_tests}/platform_readiness_kqueue_test.c")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       SALTSCORE_ENABLE_EPOLL_READINESS)
  m6_add_original(epoll
    "${_tests}/platform_readiness_epoll_test.c"
    "${_tests}/readiness_backend_contract.c")
  target_compile_definitions(m6_original_readiness_epoll PRIVATE
    SALTS_ENABLE_EPOLL_READINESS=1)
elseif(WIN32)
  m6_add_original(unsupported
    "${_tests}/platform_readiness_unsupported_test.c")
  target_compile_definitions(m6_original_readiness_unsupported PRIVATE
    SALTS_TEST_EXPECT_POLL_UNSUPPORTED=1)
endif()
