# CNet test-only link closure: same real CNet implementation as the
# installed shared target, compiled as a PRIVATE owner only for this test image.
# The executable links exactly one CNet state machine, never Salts::CNet.
if(NOT TARGET saltscore_cnet OR NOT TARGET Salts::TinyTest)
  message(FATAL_ERROR "Need actual CNet implementation and native TinyTest")
endif()
# _saltscore_cnet_sources is declared by Core at the repository root.
# Relative entries must not be interpreted relative to tests/cnet-owner;
# preserve generator-expression OBJECTS references unchanged.
set(_core_owner_native_sources)
foreach(_unit IN LISTS _saltscore_cnet_sources)
  if(_unit MATCHES "^\\$<TARGET_OBJECTS:")
    list(APPEND _core_owner_native_sources "${_unit}")
  else()
    list(APPEND _core_owner_native_sources
      "${PROJECT_SOURCE_DIR}/${_unit}")
  endif()
endforeach()
add_library(saltscore_cnet_original_test_owner STATIC
  ${_core_owner_native_sources})
target_compile_features(saltscore_cnet_original_test_owner PUBLIC c_std_11)
# Match the original first-party CNet test-profile flags exactly.
# None are installed, exported or applied to the real Salts::CNet DSO.
target_compile_definitions(saltscore_cnet_original_test_owner PUBLIC
  CNET_INTERNAL_PROFILING=1 CNET_INTERNAL_MULTI_OWNER_POC=1
  CNET_INTERNAL_TESTING=1)
set_target_properties(saltscore_cnet_original_test_owner PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
target_include_directories(saltscore_cnet_original_test_owner
  PRIVATE
    "${PROJECT_SOURCE_DIR}/core/include"
    "${PROJECT_SOURCE_DIR}/platform/include"
    "${PROJECT_SOURCE_DIR}/native-io/include"
    "${PROJECT_SOURCE_DIR}/concurrency/include"
    "${PROJECT_SOURCE_DIR}/cnet/include"
    "${PROJECT_SOURCE_DIR}/cnet/src"
    "${PROJECT_SOURCE_DIR}/cnet/src/wsparser"
    "${PROJECT_SOURCE_DIR}/native-io/src"
    "${PROJECT_SOURCE_DIR}/vendor/cnet/monocypher"
    "${PROJECT_SOURCE_DIR}/vendor/cnet/reed")
target_link_libraries(saltscore_cnet_original_test_owner
  PUBLIC Salts::Core Salts::NativeIO Salts::Concurrency
  PRIVATE Salts::URI Salts::Platform saltscore_cnet_monocypher
    saltscore_cnet_reed c-ares::cares kcp::kcp GmSSL::GmSSL
    $<$<PLATFORM_ID:Windows>:crypt32>)
if(APPLE)
  target_link_libraries(saltscore_cnet_original_test_owner PRIVATE
    "${SALTSCORE_CNET_COREFOUNDATION}"
    "${SALTSCORE_CNET_SECURITY}")
endif()

add_executable(saltscore_cnet_owner_original_test cnet_owner_test.c)
target_compile_features(saltscore_cnet_owner_original_test PRIVATE c_std_11)
set_target_properties(saltscore_cnet_owner_original_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_definitions(saltscore_cnet_owner_original_test PRIVATE
  CNET_INTERNAL_TESTING=1)
target_include_directories(saltscore_cnet_owner_original_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/core/include"
    "${PROJECT_SOURCE_DIR}/platform/include"
    "${PROJECT_SOURCE_DIR}/native-io/include"
    "${PROJECT_SOURCE_DIR}/concurrency/include"
    "${PROJECT_SOURCE_DIR}/cnet/include"
  "${PROJECT_SOURCE_DIR}/cnet/src")
target_link_libraries(saltscore_cnet_owner_original_test PRIVATE
  saltscore_cnet_original_test_owner Salts::TinyTest)
if(WIN32)
  target_link_libraries(saltscore_cnet_owner_original_test PRIVATE ws2_32)
endif()
add_test(NAME saltscore.cnet_owner_original
  COMMAND saltscore_cnet_owner_original_test)
set_tests_properties(saltscore.cnet_owner_original PROPERTIES TIMEOUT 180)

# Issue #64 (Windows IOCP only): exactly the original named numeric
# 127.0.0.1 DNS/RESOLVE -> NativeIO TCP connect -> OPEN case. TinyTest
# --filter is an actual case selector, not a replacement/mocked resolver.
# The Windows CI runs this independently 32 times and stops at FIRST RED.
# Never retry a failing network connection inside CNet itself.
if(WIN32)
  add_test(NAME saltscore.cnet_owner_original_numeric_host
    COMMAND saltscore_cnet_owner_original_test
      --filter "owns the resolve to TCP connect state transition")
  set_tests_properties(saltscore.cnet_owner_original_numeric_host
    PROPERTIES TIMEOUT 15)
endif()
