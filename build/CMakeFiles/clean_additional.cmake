# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  [[CMakeFiles\Cuebase_autogen.dir\AutogenUsed.txt]]
  [[CMakeFiles\Cuebase_autogen.dir\ParseCache.txt]]
  "Cuebase_autogen"
  )
endif()
