# Single native BER/DER and key/signature-format codec, without CMeta or
# cryptographic algorithm/provider ownership (that stays qigao/crypto).
add_library(saltscore_asn1 SHARED src/asn1.c src/value.c src/crypto.c)
add_library(Salts::ASN1 ALIAS saltscore_asn1)
target_compile_features(saltscore_asn1 PUBLIC c_std_11)
set_target_properties(saltscore_asn1 PROPERTIES
  EXPORT_NAME ASN1 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}"
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(saltscore_asn1 PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(saltscore_asn1
  PRIVATE "$<$<PLATFORM_ID:Windows>:SALTS_ASN1_API=__declspec(dllexport)>"
  INTERFACE "$<$<PLATFORM_ID:Windows>:SALTS_ASN1_API=__declspec(dllimport)>")
install(TARGETS saltscore_asn1 EXPORT SaltsCoreTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/salts DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
