# Copyright (C) 2025 Infineon Technologies
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.5)

if(CONFIG_MCUBOOTAPP)

  # Helper function to locate file, return path in "located_file" or halt with error if not found
  function(_mcubootapp_locate_file located_file name description)
    if(IS_ABSOLUTE ${name})
      cmake_path(SET ${located_file} ${name})
    else()
      find_file(${located_file} NAMES "${name}" PATHS "${APPLICATION_SOURCE_DIR}" "${WEST_TOPDIR}"
                NO_DEFAULT_PATH)
    endif()
    if(NOT IS_ABSOLUTE "${${located_file}}" OR NOT EXISTS "${${located_file}}")
      message(FATAL_ERROR "Can't find ${description} file "
                          "\"${name}\" "
                          "(Note: Relative paths are searched through "
                          "APPLICATION_SOURCE_DIR=\"${APPLICATION_SOURCE_DIR}\" "
                          "and WEST_TOPDIR=\"${WEST_TOPDIR}\")")
    endif()
    message(STATUS "Found ${description} file: ${${located_file}}")
    set(${located_file} ${${located_file}} PARENT_SCOPE)
  endfunction()

  project(mcubootapp C ASM)

  cmake_path(SET MCUBOOTAPP_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

  # Get flash partitions from device tree
  set(partitions boot_partition swap_status_partition scratch_partition slot0_partition
                 slot1_partition)
  foreach(partition ${partitions})
    dt_nodelabel(${partition} NODELABEL "${partition}")
    if(NOT ${partition})
      message(FATAL_ERROR "Flash partition with label \"${partition}\" not found in devicetree.")
    endif()
    dt_reg_size(${partition}_size PATH ${${partition}})
    dt_reg_addr(${partition}_addr PATH ${${partition}})
  endforeach()

  # Get bootstrap destination region (required when generating the app header)
  dt_nodelabel(sram_bootstrap NODELABEL "sram_bootstrap")
  if(NOT sram_bootstrap)
    message(FATAL_ERROR "Memory region with label \"sram_bootstrap\" not found in devicetree.")
  endif()
  dt_reg_addr(bootstrap_dst_addr PATH ${sram_bootstrap})
  dt_reg_size(bootstrap_size PATH ${sram_bootstrap})

  set(USE_EXTERNAL_FLASH 1)

  set(CORE "CM33")
  set(PLATFORM_SUFFIX "cyw20829")
  set(PDL_CAT_SUFFIX "1B")

  set(MCUBOOT_PATH "${ZEPHYR_MCUBOOTAPP_MODULE_DIR}/mcuboot")
  set(PRJ_DIR "${MCUBOOT_PATH}/boot/cypress")
  set(CUR_APP_PATH "${PRJ_DIR}/MCUBootApp")
  set(CY_LIBS_PATH ${PRJ_DIR}/libs)

  # If SECURE LCS then locate tools, policy and L1 key files
  if(DEFINED CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE
     AND NOT ${CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE} STREQUAL "")
    find_program(CYSECURETOOLS cysecuretools REQUIRED)
    message(STATUS "Found cysecuretools: ${CYSECURETOOLS}")

    # Locate CySecureTools policy file
    _mcubootapp_locate_file(cysecuretools_policy "${CONFIG_INFINEON_SECURE_POLICY}"
                            "CySecureTools policy")

    # Locate bootrom (L1) signature key file
    _mcubootapp_locate_file(bootrom_signature_key "${CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE}"
                            "bootrom signature key")

    if(DEFINED CONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE
       AND NOT ${CONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE} STREQUAL "")
      # Locate bootrom (L1) encryption key file
      _mcubootapp_locate_file(bootrom_encryption_key
                              "${CONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE}"
                              "bootrom encryption key")
    endif()
  endif()

  # Locate MCUboot (L2) signature key
  _mcubootapp_locate_file(signature_key_file "${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE}"
                          "MCUboot signature key")
  if("${signature_key_file}" STREQUAL
     "${ZEPHYR_MCUBOOTAPP_MODULE_DIR}/mcuboot/boot/cypress/keys/cypress-test-ec-p256.pem")
    message(WARNING "WARNING: Using default MCUboot signature key file, "
                    "this file is for debug use only and is not secure!")
  endif()

  # Generate signature pub key C source from private key
  set(autogen_sign_pubkey ${MCUBOOTAPP_OUT_DIR}/autogen-sign-pub-key.c)
  add_custom_command(OUTPUT ${autogen_sign_pubkey}
                     COMMAND ${PYTHON_EXECUTABLE} ${MCUBOOT_PATH}/scripts/imgtool.py getpub -k
                             ${signature_key_file} > ${autogen_sign_pubkey}
                     DEPENDS ${signature_key_file})

  if(CONFIG_MCUBOOT_ENCRYPTION_KEY_FILE AND NOT CONFIG_MCUBOOT_ENCRYPTION_KEY_FILE STREQUAL "")
    set(ENC_IMG 1)

    # Locate MCUboot (L2) encryption key
    _mcubootapp_locate_file(encryption_key_file "${CONFIG_MCUBOOT_ENCRYPTION_KEY_FILE}"
                            "MCUboot encryption key")

    # Generate encryption priv key C source from private key
    set(autogen_enc_privkey ${MCUBOOTAPP_OUT_DIR}/autogen-enc-priv-key.c)
    add_custom_command(OUTPUT ${autogen_enc_privkey}
                       COMMAND ${PYTHON_EXECUTABLE} ${MCUBOOT_PATH}/scripts/imgtool.py getpriv -k
                               ${encryption_key_file} > ${autogen_enc_privkey}
                       DEPENDS ${encryption_key_file})
  endif()

  add_compile_options(--specs=nano.specs -mcpu=cortex-m33+nodsp -mthumb -ffunction-sections
                      -fdata-sections -Wall -Wextra -Werror -Og -g3)
  add_link_options(--specs=nano.specs
                   -mcpu=cortex-m33
                   -Wl,--gc-sections
                   -Wl,--fatal-warnings
                   -Wl,--defsym,BOOTLOADER_SIZE=${boot_partition_size}
                   -Wl,-Map=${MCUBOOTAPP_OUT_DIR}/mcubootapp.map
                   -T
                   ${MCUBOOTAPP_OUT_DIR}/mcubootapp.ld)

  add_compile_definitions(MCUBOOT_FIH_PROFILE_ON
                          MCUBOOT_FIH_PROFILE_MEDIUM
                          CY_FLASH_MAP_JSON
                          BOOT_CM33
                          APP_CM33
                          MCUBOOT_IMAGE_NUMBER=1
                          USE_SHARED_SLOT=0
                          MCUBOOT_PLATFORM_CHUNK_SIZE=4096U
                          MEMORY_ALIGN=0x1000
                          PLATFORM_MAX_TRAILER_PAGE_SIZE=0x1000
                          USE_XIP
                          CY_BOOT_USE_EXTERNAL_FLASH
                          CY_MAX_EXT_FLASH_ERASE_SIZE=4096U
                          MCUBOOT_BOOTSTRAP
                          MCUBOOT_MAX_IMG_SECTORS=96
                          MCUBOOT_LOG_LEVEL=MCUBOOT_LOG_LEVEL_DEBUG
                          CY_CRYPTO_HAL_DISABLE
                          CY_MBEDTLS_HW_ACCELERATION
                          cy_stc_cryptolite_context_sha256_t=cy_stc_cryptolite_context_sha_t
                          MCUBootApp
                          CYW20829B0LKML
                          USE_SWAP_STATUS=1
                          COMPONENT_CUSTOM_DESIGN_MODUS
                          CYW20829
                          CY_USING_HAL
                          COMPONENT_CM33
                          COMPONENT_PSOC6HAL
                          COMPONENT_SOFTFP
                          FLASH_BOOT
                          COMPONENT_CAT1
                          COMPONENT_CAT1B)
  if(ENC_IMG EQUAL 1)
    add_compile_definitions(ENC_IMG=1 MCUBOOT_ENC_IMAGES_XIP)
  endif()

  # ################################################################################################
  # Memory map generation
  # ################################################################################################

  add_library(memorymap INTERFACE)
  target_include_directories(memorymap INTERFACE ${MCUBOOTAPP_OUT_DIR})

  # Get boot_partition from device tree
  math(EXPR BOOTLOADER_ADDRESS "${boot_partition_addr} + ${CONFIG_CYW20829_FLASH_SAHB_ADDR}"
       OUTPUT_FORMAT HEXADECIMAL)
  set_target_properties(memorymap PROPERTIES BOOTLOADER_ADDRESS ${BOOTLOADER_ADDRESS}
                                             BOOTLOADER_SIZE ${boot_partition_size})
  # Get swap_status_partition from device tree
  math(EXPR BOOTLOADER_STATUS_ADDRESS
       "${swap_status_partition_addr} + ${CONFIG_CYW20829_FLASH_SAHB_ADDR}"
       OUTPUT_FORMAT HEXADECIMAL)
  set_target_properties(memorymap PROPERTIES BOOTLOADER_STATUS_ADDRESS ${BOOTLOADER_STATUS_ADDRESS}
                                             BOOTLOADER_STATUS_SIZE ${swap_status_partition_size})
  # Get scratch_partition from device tree
  math(EXPR BOOTLOADER_SCRATCH_ADDRESS
       "${scratch_partition_addr} + ${CONFIG_CYW20829_FLASH_SAHB_ADDR}" OUTPUT_FORMAT HEXADECIMAL)
  set_target_properties(memorymap
                        PROPERTIES BOOTLOADER_SCRATCH_ADDRESS ${BOOTLOADER_SCRATCH_ADDRESS}
                                   BOOTLOADER_SCRATCH_SIZE ${scratch_partition_size})
  # Get slot0_partition from device tree
  math(EXPR SLOT0_PARTITION_ADDRESS "${slot0_partition_addr} + ${CONFIG_CYW20829_FLASH_SAHB_ADDR}"
       OUTPUT_FORMAT HEXADECIMAL)
  set_target_properties(memorymap PROPERTIES APPLICATION_1_ADDRESS ${SLOT0_PARTITION_ADDRESS}
                                             APPLICATION_1_SIZE ${slot0_partition_size})
  # Get slot1_partition from device tree
  math(EXPR SLOT1_PARTITION_ADDRESS "${slot1_partition_addr} + ${CONFIG_CYW20829_FLASH_SAHB_ADDR}"
       OUTPUT_FORMAT HEXADECIMAL)
  set_target_properties(memorymap
                        PROPERTIES APPLICATION_1_UPGRADE_ADDRESS ${SLOT1_PARTITION_ADDRESS}
                                   APPLICATION_1_UPGRADE_SIZE ${slot1_partition_size})

  # Generate flash_map.json
  file(GENERATE OUTPUT ${MCUBOOTAPP_OUT_DIR}/flash_map.json
       INPUT ${CMAKE_CURRENT_LIST_DIR}/flash_map_template.json)

  # Command to generate memorymap.c/h from flash_map.json
  add_custom_command(OUTPUT ${MCUBOOTAPP_OUT_DIR}/memorymap.c ${MCUBOOTAPP_OUT_DIR}/memorymap.h
                     DEPENDS ${MCUBOOTAPP_OUT_DIR}/flash_map.json
                     COMMAND ${PYTHON_EXECUTABLE} ${PRJ_DIR}/scripts/memorymap.py ARGS -p CYW20829
                             -i ${MCUBOOTAPP_OUT_DIR}/flash_map.json -o
                             ${MCUBOOTAPP_OUT_DIR}/memorymap.c -a ${MCUBOOTAPP_OUT_DIR}/memorymap.h
                             > ${MCUBOOTAPP_OUT_DIR}/legacy_unused.mk
                     BYPRODUCTS ${MCUBOOTAPP_OUT_DIR}/legacy_unused.mk)

  # ################################################################################################
  # Executable
  # ################################################################################################

  # Target for final hex file
  add_custom_target(mcubootapp DEPENDS ${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.hex)
  set_target_properties(mcubootapp PROPERTIES EXCLUDE_FROM_ALL FALSE OUTPUT_NAME "mcubootapp.final"
                                              RUNTIME_OUTPUT_DIRECTORY ${MCUBOOTAPP_OUT_DIR})

  set(CMAKE_EXECUTABLE_SUFFIX "")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${MCUBOOTAPP_OUT_DIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${MCUBOOTAPP_OUT_DIR})

  # Generate linker script with constants based on memory configuration in device tree
  configure_file(${CMAKE_CURRENT_LIST_DIR}/mcubootapp_template.ld
                 ${MCUBOOTAPP_OUT_DIR}/mcubootapp.ld)

  # Step 1 build mcubootapp.elf
  add_executable(mcubootapp_elf EXCLUDE_FROM_ALL)
  set_target_properties(mcubootapp_elf
                        PROPERTIES OUTPUT_NAME "mcubootapp.elf"
                                   ADDITIONAL_CLEAN_FILES "${MCUBOOTAPP_OUT_DIR}/mcubootapp.map")
  target_link_libraries(mcubootapp_elf
                        PRIVATE mbedtls retarget-io mtb-pdl-cat1 mtb-hal-cat1 core-lib_headers
                                cmsis_headers bootutil platform mcubootapp-config)

  file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${CUR_APP_PATH}/*.h)
  target_sources(mcubootapp_elf PRIVATE ${sources} ${CUR_APP_PATH}/main.c
                                        ${CMAKE_CURRENT_LIST_DIR}/keys.c ${autogen_sign_pubkey})
  if(ENC_IMG EQUAL 1)
    target_sources(mcubootapp_elf PRIVATE ${autogen_enc_privkey})
  endif()

  target_include_directories(mcubootapp_elf PRIVATE ${PRJ_DIR} ${PRJ_DIR}/platforms/utils/CYW20829)

  # Step 2 mcubootapp.elf -> mcubootapp.bin/hex
  add_custom_command(OUTPUT ${MCUBOOTAPP_OUT_DIR}/mcubootapp.bin
                            ${MCUBOOTAPP_OUT_DIR}/mcubootapp.hex
                     DEPENDS mcubootapp_elf
                     WORKING_DIRECTORY ${MCUBOOTAPP_OUT_DIR}
                     COMMENT "Generating mcubootapp.hex from mcubootapp.elf"
                     COMMAND ${CMAKE_OBJCOPY} mcubootapp.elf -S -O ihex mcubootapp.hex
                             --remove-section .cy_sflash_user_data --remove-section .cy_toc_part2
                     COMMAND ${CMAKE_OBJCOPY} mcubootapp.elf -S -O binary mcubootapp.bin
                             --remove-section .cy_sflash_user_data --remove-section .cy_toc_part2)

  if(DEFINED CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE
     AND NOT ${CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE} STREQUAL ""
     AND DEFINED CONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE
     AND NOT ${CONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE} STREQUAL "")
    set(unmerged_image_name "mcubootapp.signed_encrypted")
  elseif(DEFINED CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE
         AND NOT ${CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE} STREQUAL "")
    set(unmerged_image_name "mcubootapp.signed")
  else()
    set(unmerged_image_name "mcubootapp")
  endif()

  set(gen_app_header_args)
  set(gen_app_header_depends)

  if(DEFINED CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE
     AND NOT ${CONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE} STREQUAL "")
    set(gen_app_header_args ${gen_app_header_args} --secure_lcs True)

    if(DEFINED CONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE
       AND NOT ${CONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE} STREQUAL "")
      set(gen_app_header_args ${gen_app_header_args} --smif-config
                              ${MCUBOOTAPP_OUT_DIR}/nonce-output.bin)
      set(enc_option --encrypt --nonce-output ${MCUBOOTAPP_OUT_DIR}/nonce-output.bin --enckey
                     ${bootrom_encryption_key})
      set(gen_app_header_depends ${MCUBOOTAPP_OUT_DIR}/nonce-output.bin)
    endif()

    set(bin2hex_option bin2hex --image ${MCUBOOTAPP_OUT_DIR}/${unmerged_image_name}.bin --output
                       ${MCUBOOTAPP_OUT_DIR}/${unmerged_image_name}.hex --offset 0x60000030)

    # Step 3 (optional) mcubootapp.bin -> mcubootapp.signed.bin/hex and
    # mcubootapp.signed_encrypted.bin/hex
    add_custom_command(OUTPUT ${MCUBOOTAPP_OUT_DIR}/${unmerged_image_name}.hex
                              ${MCUBOOTAPP_OUT_DIR}/mcubootapp.signed.bin
                              ${MCUBOOTAPP_OUT_DIR}/mcubootapp.signed_encrypted.bin
                              ${MCUBOOTAPP_OUT_DIR}/nonce-output.bin
                       COMMENT "Generating ${unmerged_image_name}.bin/hex"
                       DEPENDS ${MCUBOOTAPP_OUT_DIR}/mcubootapp.bin
                       COMMAND ${CYSECURETOOLS} -q -t cyw20829 -p ${cysecuretools_policy}
                               sign-image --image-format bootrom_next_app -i
                               ${MCUBOOTAPP_OUT_DIR}/mcubootapp.bin -o
                               ${MCUBOOTAPP_OUT_DIR}/mcubootapp.signed.bin --slot-size
                               ${CONFIG_FLASH_LOAD_SIZE} --app-addr 0x08000030 ${enc_option}
                               --key-path ${bootrom_signature_key} ${bin2hex_option})
  endif()

  set(gen_app_header_args ${gen_app_header_args} --flash_addr_offset
                          ${CONFIG_CYW20829_FLASH_SAHB_ADDR})

  # Step 4 generate app header
  add_custom_command(OUTPUT ${MCUBOOTAPP_OUT_DIR}/app_header.bin
                            ${MCUBOOTAPP_OUT_DIR}/app_header.hex
                     DEPENDS ${gen_app_header_depends}
                     COMMAND ${PYTHON_EXECUTABLE}
                             ${ZEPHYR_BASE}/soc/infineon/cat1b/cyw20829/gen_app_header.py -p
                             ${MCUBOOTAPP_OUT_DIR} -n unused_prj_name ${gen_app_header_args}
                             --bootstrap-size ${bootstrap_size} --bootstrap-dst-addr
                             ${bootstrap_dst_addr})

  # Step 5 prepend app header and create mcubootapp.final.hex
  add_custom_command(OUTPUT ${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.hex
                     DEPENDS ${MCUBOOTAPP_OUT_DIR}/${unmerged_image_name}.hex
                             ${MCUBOOTAPP_OUT_DIR}/app_header.hex
                     WORKING_DIRECTORY ${MCUBOOTAPP_OUT_DIR}
                     COMMENT "Generating mcubootapp.final.hex from ${unmerged_image_name}.hex"
                     # Merge platform specific header and zephyr image to a single binary.
                     COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/scripts/build/mergehex.py -o
                             ${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.hex
                             ${MCUBOOTAPP_OUT_DIR}/${unmerged_image_name}.hex
                             ${MCUBOOTAPP_OUT_DIR}/app_header.hex
                     COMMAND ${CMAKE_OBJCOPY} --change-address=${CONFIG_CYW20829_FLASH_SAHB_ADDR} -I
                             ihex -O binary --set-start 0x60000000 mcubootapp.final.hex
                             mcubootapp.final.bin
                     BYPRODUCTS ${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.bin)

  # ################################################################################################
  # mcubootapp-config library
  # ################################################################################################
  add_library(mcubootapp-config INTERFACE)
  target_include_directories(mcubootapp-config INTERFACE ${CUR_APP_PATH}/config ${CUR_APP_PATH})

  # ################################################################################################
  # cmsis library
  # ################################################################################################
  add_library(cmsis_headers INTERFACE)
  target_include_directories(cmsis_headers INTERFACE ${CY_LIBS_PATH}/cmsis/Core/Include)

  # ################################################################################################
  # core-lib library
  # ################################################################################################
  add_library(core-lib_headers INTERFACE)
  target_include_directories(core-lib_headers INTERFACE ${CY_LIBS_PATH}/core-lib/include)

  # ################################################################################################
  # mtb-hal-cat1 library
  # ################################################################################################
  add_library(mtb-hal-cat1 STATIC EXCLUDE_FROM_ALL )
  target_link_libraries(mtb-hal-cat1 PRIVATE mtb-pdl-cat1 cmsis_headers core-lib_headers platform)
  file(GLOB_RECURSE
       mtb-hal-cat1-sources
       CONFIGURE_DEPENDS
       ${CY_LIBS_PATH}/mtb-hal-cat1/source/*.h
       ${CY_LIBS_PATH}/mtb-hal-cat1/source/*.c
       ${CY_LIBS_PATH}/mtb-hal-cat1/COMPONENT_CAT${PDL_CAT_SUFFIX}/source/pin_packages/*.h
       ${CY_LIBS_PATH}/mtb-hal-cat1/COMPONENT_CAT${PDL_CAT_SUFFIX}/source/pin_packages/*.c
       ${CY_LIBS_PATH}/mtb-hal-cat1/COMPONENT_CAT${PDL_CAT_SUFFIX}/source/triggers/*.h
       ${CY_LIBS_PATH}/mtb-hal-cat1/COMPONENT_CAT${PDL_CAT_SUFFIX}/source/triggers/*.c)
  target_sources(mtb-hal-cat1 PRIVATE ${mtb-hal-cat1-sources})
  target_include_directories(mtb-hal-cat1
                             PUBLIC ${CY_LIBS_PATH}/mtb-hal-cat1/include
                                    ${CY_LIBS_PATH}/mtb-hal-cat1/include_pvt
                                    ${CY_LIBS_PATH}/mtb-hal-cat1/COMPONENT_CAT1B/include)

  # ################################################################################################
  # mtb-pdl-cat1 library
  # ################################################################################################
  add_library(mtb-pdl-cat1 STATIC EXCLUDE_FROM_ALL )
  target_link_libraries(mtb-pdl-cat1 PRIVATE cmsis_headers core-lib_headers platform)
  file(GLOB_RECURSE
       mtb-pdl-cat1-sources
       CONFIGURE_DEPENDS
       ${CY_LIBS_PATH}/mtb-pdl-cat1/drivers/source/TOOLCHAIN_GCC_ARM/cy_syslib_ext.S
       ${CY_LIBS_PATH}/mtb-pdl-cat1/drivers/source/*.h
       ${CY_LIBS_PATH}/mtb-pdl-cat1/drivers/source/*.c
       ${CY_LIBS_PATH}/mtb-pdl-cat1/devices/COMPONENT_CAT${PDL_CAT_SUFFIX}/source/*.h
       ${CY_LIBS_PATH}/mtb-pdl-cat1/devices/COMPONENT_CAT${PDL_CAT_SUFFIX}/source/*.c)
  target_sources(mtb-pdl-cat1 PRIVATE ${mtb-pdl-cat1-sources})
  target_include_directories(mtb-pdl-cat1
                             PUBLIC ${CY_LIBS_PATH}/mtb-pdl-cat1/drivers/include
                                    ${CY_LIBS_PATH}/mtb-pdl-cat1/devices/COMPONENT_CAT1B/include)

  # ################################################################################################
  # retarget-io library
  # ################################################################################################
  add_library(retarget-io STATIC EXCLUDE_FROM_ALL ${CY_LIBS_PATH}/retarget-io/cy_retarget_io.c
                                                  ${CY_LIBS_PATH}/retarget-io/cy_retarget_io.h)
  target_link_libraries(retarget-io PRIVATE mtb-hal-cat1 mtb-pdl-cat1 core-lib_headers
                                            cmsis_headers platform)
  target_include_directories(retarget-io PUBLIC ${CY_LIBS_PATH}/retarget-io)

  # ################################################################################################
  # cy-mbedtls-acceleration library
  # ################################################################################################
  if(NOT DEFINED CRYPTO_ACC_TYPE)
    set(CRYPTO_ACC_TYPE "MXCRYPTOLITE")
  endif()
  add_library(cy-mbedtls-acceleration STATIC EXCLUDE_FROM_ALL
              ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_${CRYPTO_ACC_TYPE}/aes_alt_mxcryptolite.c
              ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_${CRYPTO_ACC_TYPE}/cryptolite_common.h
              ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_${CRYPTO_ACC_TYPE}/sha256_alt_mxcryptolite.c
              ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_${CRYPTO_ACC_TYPE}/trng_alt_mxcryptolite.c
              ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_${CRYPTO_ACC_TYPE}/aes_alt_mxcryptolite.h
              ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_${CRYPTO_ACC_TYPE}/ecdsa_alt_mxcryptolite.c
              ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_${CRYPTO_ACC_TYPE}/sha256_alt_mxcryptolite.h
  )
  target_link_libraries(cy-mbedtls-acceleration PRIVATE mbedtls mtb-pdl-cat1 core-lib_headers
                                                        cmsis_headers platform mcubootapp-config)
  target_include_directories(cy-mbedtls-acceleration
                             PUBLIC ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/include
                                    ${CY_LIBS_PATH}/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_MXCRYPTOLITE
  )

  # ################################################################################################
  # mbedtls library
  # ################################################################################################
  set(MBEDTLS_PATH ${PRJ_DIR}/../../ext)
  add_library(mbedtls STATIC EXCLUDE_FROM_ALL )
  file(GLOB_RECURSE mbedtls-sources CONFIGURE_DEPENDS ${MBEDTLS_PATH}/mbedtls/library/*.h
       ${MBEDTLS_PATH}/mbedtls/library/*.c)
  target_sources(mbedtls PRIVATE ${mbedtls-sources})
  target_link_libraries(mbedtls PRIVATE cy-mbedtls-acceleration mtb-pdl-cat1 core-lib_headers
                                        cmsis_headers platform mcubootapp-config)
  target_include_directories(mbedtls
                             PUBLIC ${MBEDTLS_PATH}/mbedtls/include
                                    ${MBEDTLS_PATH}/mbedtls/include/mbedtls
                                    ${MBEDTLS_PATH}/mbedtls/include/psa
                                    ${MBEDTLS_PATH}/mbedtls/library)
  target_compile_options(mbedtls PUBLIC -DMBEDTLS_USER_CONFIG_FILE=\"mcuboot_crypto_acc_config.h\"
                                        -DMBEDTLS_CONFIG_FILE=\"mcuboot_crypto_config.h\")

  # ################################################################################################
  # platform library
  # ################################################################################################
  set(FAMILY "CYW20829")
  set(COMPONENT_CORE_PATH ${PRJ_DIR}/platforms/BSP/${FAMILY}/system/COMPONENT_${CORE})
  add_library(platform STATIC EXCLUDE_FROM_ALL )
  add_dependencies(platform memorymap)
  target_link_libraries(platform PRIVATE bootutil mtb-pdl-cat1 mtb-hal-cat1 core-lib_headers
                                         cmsis_headers mcubootapp-config mbedtls)
  file(GLOB
       platform-sources
       CONFIGURE_DEPENDS
       ${PRJ_DIR}/platforms/BSP/${FAMILY}/*.h
       ${PRJ_DIR}/platforms/BSP/${FAMILY}/*.c
       ${PRJ_DIR}/platforms/security_counter/*.h
       ${PRJ_DIR}/platforms/security_counter/*.c
       ${PRJ_DIR}/platforms/security_counter/${FAMILY}/*.h
       ${PRJ_DIR}/platforms/security_counter/${FAMILY}/*.c
       ${PRJ_DIR}/platforms/memory/${FAMILY}/*.h
       ${PRJ_DIR}/platforms/memory/${FAMILY}/*.c)
  target_sources(platform
                 PRIVATE ${COMPONENT_CORE_PATH}/ns_system_${PLATFORM_SUFFIX}.c
                         ${COMPONENT_CORE_PATH}/ns_start_${PLATFORM_SUFFIX}.c
                         ${platform-sources}
                         ${PRJ_DIR}/platforms/memory/cy_flash_map.c
                         ${PRJ_DIR}/platforms/utils/${FAMILY}/cyw_platform_utils.c
                         # Generated memory map files
                         ${MCUBOOTAPP_OUT_DIR}/memorymap.c
                         ${MCUBOOTAPP_OUT_DIR}/memorymap.h)
  if(${USE_EXTERNAL_FLASH} EQUAL 1)
    file(GLOB_RECURSE
         platform-sources
         CONFIGURE_DEPENDS
         ${PRJ_DIR}/platforms/memory/external_memory/*.h
         ${PRJ_DIR}/platforms/memory/external_memory/*.c
         ${PRJ_DIR}/platforms/memory/${FAMILY}/flash_qspi/*.h
         ${PRJ_DIR}/platforms/memory/${FAMILY}/flash_qspi/*.c)
    target_sources(platform PRIVATE ${platform-sources})
  endif()
  # cy_service_app.c is including bootutil_priv.h located in bootutil/src
  set_source_files_properties(${PRJ_DIR}/platforms/security_counter/CYW20829/cy_service_app.c
                              PROPERTIES INCLUDE_DIRECTORIES ${MCUBOOT_PATH}/boot/bootutil/src)
  target_include_directories(platform
                             PUBLIC ${PRJ_DIR}/platforms/BSP/CYW20829/system/COMPONENT_CM33/HEADER_FILES
                                    ${PRJ_DIR}/platforms/BSP/CYW20829
                                    ${PRJ_DIR}/platforms/BSP/CYW20829/system
                                    ${PRJ_DIR}/platforms/security_counter
                                    ${PRJ_DIR}/platforms/security_counter/CYW20829
                                    ${PRJ_DIR}/platforms/memory
                                    ${PRJ_DIR}/platforms/memory/CYW20829
                                    ${PRJ_DIR}/platforms/memory/CYW20829/include
                                    ${PRJ_DIR}/platforms/memory/CYW20829/flash_qspi
                                    ${PRJ_DIR}/platforms/memory/external_memory
                                    ${PRJ_DIR}/platforms/memory/flash_map_backend
                                    ${PRJ_DIR}/platforms/crypto/CYW20829
                                    # Generated memory map file memorymap.h
                                    ${MCUBOOTAPP_OUT_DIR})

  # ################################################################################################
  # bootutil library
  # ################################################################################################
  add_library(bootutil STATIC EXCLUDE_FROM_ALL )

  target_sources(bootutil
                 PRIVATE ${MCUBOOT_PATH}/boot/bootutil/src/boot_record.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/bootutil_misc.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/bootutil_public.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/caps.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/encrypted.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/fault_injection_hardening.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/image_ec.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/image_ec256.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/image_ed25519.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/image_rsa.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/image_validate.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/loader.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/swap_misc.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/swap_move.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/swap_scratch.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/tlv.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/crc32c.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/swap_status.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/swap_status_misc.c
                         ${MCUBOOT_PATH}/boot/bootutil/src/swap_status_part.c
                         # Generated memory map file
                         ${MCUBOOTAPP_OUT_DIR}/memorymap.h)
  target_link_libraries(bootutil PRIVATE platform mtb-pdl-cat1 core-lib_headers mbedtls
                                         cy-mbedtls-acceleration cmsis_headers mcubootapp-config)
  target_include_directories(bootutil PUBLIC ${MCUBOOT_PATH}/boot/bootutil/include
                             PRIVATE ${MCUBOOT_PATH}/boot/bootutil/src)

  # ################################################################################################
  # Target for flashing final hex
  # ################################################################################################
  add_custom_target(mcubootapp-flash
                    DEPENDS "${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.hex"
                    COMMAND west flash --skip-rebuild --hex-file
                            "${MCUBOOTAPP_OUT_DIR}/mcubootapp.final.hex" -d
                            "${APPLICATION_BINARY_DIR}"
                    USES_TERMINAL)

endif()
