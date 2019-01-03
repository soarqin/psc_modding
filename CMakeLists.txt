cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1")

add_subdirectory(src/miniz)
add_subdirectory(src/bootstrap)
