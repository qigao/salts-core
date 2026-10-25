cmake_minimum_required(VERSION 3.20)
project(SaltsCoreABIExpectedRejection LANGUAGES C)
# This is deliberately incompatible with installed pre-stable 0.1.
find_package(SaltsCore 0.2 CONFIG REQUIRED)
message(FATAL_ERROR "A 0.2 request unexpectedly resolved a 0.1 runtime")
