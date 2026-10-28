# Merge the already compiled patched GmSSL archive with all locally built
# Crypto/TLS/CipherBIO/SHA256/Ed448 objects into ONE deployable static archive.
# The component libraries are staging inputs, never installed or propagated.
cmake_minimum_required(VERSION 3.21)
foreach(_arg IN ITEMS ARCHIVE GMSSL_ARCHIVE ARCHIVER)
  if(NOT DEFINED ${_arg} OR "${${_arg}}" STREQUAL "")
    message(FATAL_ERROR "Crypto static merge missing ${_arg}")
  endif()
endforeach()
if(NOT EXISTS "${ARCHIVE}" OR NOT EXISTS "${GMSSL_ARCHIVE}")
  message(FATAL_ERROR "Crypto static merge requires both input archives")
endif()
set(_own "${ARCHIVE}.objects")
file(RENAME "${ARCHIVE}" "${_own}")
if(WIN32)
  execute_process(COMMAND "${ARCHIVER}" "/NOLOGO" "/OUT:${ARCHIVE}"
    "${_own}" "${GMSSL_ARCHIVE}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
elseif(APPLE)
  execute_process(COMMAND "/usr/bin/libtool" -static -o "${ARCHIVE}"
    "${_own}" "${GMSSL_ARCHIVE}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
else()
  set(_mri "${ARCHIVE}.mri")
  file(WRITE "${_mri}" "CREATE \"${ARCHIVE}\"\nADDLIB \"${_own}\"\nADDLIB \"${GMSSL_ARCHIVE}\"\nSAVE\nEND\n")
  execute_process(COMMAND "${ARCHIVER}" -M INPUT_FILE "${_mri}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
  file(REMOVE "${_mri}")
  if(_rc EQUAL 0 AND DEFINED RANLIB AND NOT RANLIB STREQUAL "")
    execute_process(COMMAND "${RANLIB}" "${ARCHIVE}"
      RESULT_VARIABLE _ranlib_rc ERROR_VARIABLE _ranlib_error)
    if(NOT _ranlib_rc EQUAL 0)
      message(FATAL_ERROR "Crypto static archive ranlib failed: ${_ranlib_error}")
    endif()
  endif()
endif()
if(NOT _rc EQUAL 0 OR NOT EXISTS "${ARCHIVE}")
  message(FATAL_ERROR "Crypto static archive merge failed: ${_out}\n${_err}")
endif()
file(REMOVE "${_own}")
