# Copyright (C) 2025 Infineon Technologie
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.5)

cmake_path(SET MCUBOOTAPP_OUT_DIR ${CMAKE_BINARY_DIR}/modules/mcubootapp)

function(app_set_runner_args)
  set_property(TARGET runners_yaml_props_target PROPERTY elf_file
                                                         ${MCUBOOTAPP_OUT_DIR}/mcubootapp.elf)
  board_runner_args(openocd "--hex-file=${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.hex")
  board_runner_args(openocd "--cmd-erase=flash erase_sector 0 0 last")
  board_runner_args(jlink "--hex-file=${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.hex")
endfunction()

set(EXTRA_DTC_OVERLAY_FILE ../snippets/mcubootapp/cyw920829m2evk_02.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)
set_property(TARGET app PROPERTY EXCLUDE_FROM_ALL TRUE)

# Need a dummy source file for app target
get_property(app_out_dir TARGET app PROPERTY ARCHIVE_OUTPUT_DIRECTORY)
cmake_path(ABSOLUTE_PATH app_out_dir BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
file(WRITE ${app_out_dir}/dummy.c "int main() { return 0; }")
target_sources(app PRIVATE ${app_out_dir}/dummy.c)

# Exclude all zephyr targets from "all" target
zephyr_get_targets(${ZEPHYR_BASE} "STATIC_LIBRARY;OBJECT_LIBRARY;EXECUTABLE;UTILITY" all_targets)
foreach(target ${all_targets})
  set_property(TARGET ${target} PROPERTY EXCLUDE_FROM_ALL TRUE)
endforeach()

# Include mcubootapp target in "all" target
set_property(TARGET mcubootapp PROPERTY EXCLUDE_FROM_ALL FALSE)
