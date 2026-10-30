
cmake_minimum_required(VERSION 3.20)
project(SaltsCoreFSInstalled LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)
if(NOT TARGET Salts::FS)
  message(FATAL_ERROR "Installed Salts::FS not found")
endif()
add_executable(fs_installed_c ../test_fs.c)
target_link_libraries(fs_installed_c PRIVATE Salts::FS)
set_target_properties(fs_installed_c PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.fs.installed_c COMMAND fs_installed_c)
add_executable(fs_installed_cpp fs_cpp.cpp)
target_link_libraries(fs_installed_cpp PRIVATE Salts::FS)
set_target_properties(fs_installed_cpp PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME saltscore.fs.installed_cpp COMMAND fs_installed_cpp)

add_executable(fs_watch_installed_c fs_watch_c.c)
target_link_libraries(fs_watch_installed_c PRIVATE Salts::FS)
set_target_properties(fs_watch_installed_c PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.fs.watch_installed_c COMMAND fs_watch_installed_c)

add_executable(fs_watch_installed_cpp fs_watch_cpp.cpp)
target_link_libraries(fs_watch_installed_cpp PRIVATE Salts::FS)
set_target_properties(fs_watch_installed_cpp PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME saltscore.fs.watch_installed_cpp COMMAND fs_watch_installed_cpp)
