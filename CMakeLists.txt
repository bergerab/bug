cmake_minimum_required(VERSION 3.10)
project(bug C)

add_compile_options(-pedantic -Wall -std=c89)

add_executable(bug 
  src/bug.c 
  src/marshal.c 
  src/dynamic_byte_array.c 
  src/dynamic_array.c
  src/debug.c)
target_link_libraries(bug ffi m)