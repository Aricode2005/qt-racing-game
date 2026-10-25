# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "ApexVelocity_autogen"
  "CMakeFiles/ApexVelocity_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ApexVelocity_autogen.dir/ParseCache.txt"
  "CMakeFiles/capture_screenshots_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/capture_screenshots_autogen.dir/ParseCache.txt"
  "capture_screenshots_autogen"
  )
endif()
