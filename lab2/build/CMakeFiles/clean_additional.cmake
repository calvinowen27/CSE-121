# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "lab2.map"
  "project_elf_src_esp32c3.c"
  )
endif()
