set(SALTS_SERIAL_TEST_SOURCES)
foreach(source IN LISTS SALTS_SERIAL_SOURCES)
  if(IS_ABSOLUTE "${source}")
    list(APPEND SALTS_SERIAL_TEST_SOURCES "${source}")
  else()
    list(APPEND SALTS_SERIAL_TEST_SOURCES "${SALTS_SERIAL_SOURCE_DIR}/${source}")
  endif()
endforeach()

add_executable(test_serialport test_salts_serial.c)
target_sources(test_serialport PRIVATE ${SALTS_SERIAL_TEST_SOURCES})
target_include_directories(test_serialport
  PRIVATE
    "${SALTS_SERIAL_BINARY_DIR}"
    "${SALTS_SERIAL_SOURCE_DIR}")
target_compile_definitions(test_serialport
  PRIVATE SALTS_SERIAL_EXPORTS ${SALTS_SERIAL_PRIVATE_DEFS})
target_link_libraries(test_serialport
  PRIVATE Salts::Core Salts::CSTL Salts::TinyTest ${SALTS_SERIAL_PLATFORM_LIBS})
add_test(NAME test_serialport COMMAND test_serialport)
set_target_properties(test_serialport PROPERTIES FOLDER "salts_serial/tests")

add_executable(test_salts_serial_public_abi test_salts_serial_public_abi.c)
target_link_libraries(test_salts_serial_public_abi
  PRIVATE ${TARGET_NAME} Salts::TinyTest)
add_test(NAME test_salts_serial_public_abi COMMAND test_salts_serial_public_abi)
set_target_properties(test_salts_serial_public_abi PROPERTIES FOLDER "salts_serial/tests")

add_executable(test_salts_serial_mock test_salts_serial_mock.c)
target_sources(test_salts_serial_mock PRIVATE ${SALTS_SERIAL_TEST_SOURCES})
target_include_directories(test_salts_serial_mock
  PRIVATE
    "${SALTS_SERIAL_BINARY_DIR}"
    "${SALTS_SERIAL_SOURCE_DIR}")
target_compile_definitions(test_salts_serial_mock
  PRIVATE SALTS_SERIAL_EXPORTS ${SALTS_SERIAL_PRIVATE_DEFS} SALTS_SERIAL_TESTING)
target_link_libraries(test_salts_serial_mock
  PRIVATE Salts::Core Salts::CSTL Salts::TinyMock ${SALTS_SERIAL_PLATFORM_LIBS})
add_test(NAME test_salts_serial_mock COMMAND test_salts_serial_mock)
set_target_properties(test_salts_serial_mock PROPERTIES FOLDER "salts_serial/tests")
