# ConcatFiles.cmake
# Define in_files, out_file

math(EXPR out_arg_num "${CMAKE_ARGC} - 1")
math(EXPR last_in_arg_num "${CMAKE_ARGC} - 2")

set(out_file ${CMAKE_ARGV${out_arg_num}})

message("out_file: ${out_file}")

foreach(_arg RANGE 1 ${out_arg_num})
  message("${_arg}: ${CMAKE_ARGV${_arg}}")
endforeach()

file(REMOVE ${out_file})
foreach(_arg RANGE 3 ${last_in_arg_num})
  file(READ ${CMAKE_ARGV${_arg}} contents)
  file(APPEND ${out_file} "${contents}")
endforeach()

