cmake_minimum_required(VERSION 3.20)
project(SaltsCryptoOriginalInstalled LANGUAGES C CXX)

include(CTest)
find_package(Crypto 0.1 CONFIG REQUIRED)
if(NOT TARGET Salts::Crypto)
  message(FATAL_ERROR "Installed Salts::Crypto target is missing")
endif()

add_executable(crypto_original_consumer_c main.c surface_smoke.c)
target_link_libraries(crypto_original_consumer_c PRIVATE Salts::Crypto)
set_target_properties(crypto_original_consumer_c PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME crypto.original_installed_c11 COMMAND crypto_original_consumer_c)

add_executable(crypto_original_consumer_cpp main.cpp)
target_link_libraries(crypto_original_consumer_cpp PRIVATE Salts::Crypto)
set_target_properties(crypto_original_consumer_cpp PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME crypto.original_installed_cpp17 COMMAND crypto_original_consumer_cpp)
