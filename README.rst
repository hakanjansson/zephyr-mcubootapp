zephyr-mcubootapp
#################

This Zephyr module is an example of how the Infineon custom MCUboot based bootloader application
`MCUBootApp for the CYW20829 platform`_ can be built in a Zephyr environment.

Quick Start: Blinky with MCUBootApp bootloader
**********************************************

.. code-block:: bash

   # From the root of the zephyr repository
   cd ~/zephyrproject/zephyr

   # Add this project to the west manifest by adding hakanjansson.yaml submanifest
   wget https://raw.githubusercontent.com/hakanjansson/zephyr-manifest/refs/heads/main/hakanjansson.yaml -O submanifests/hakanjansson.yaml

   # Fetch this project/module
   west update

   # Build blinky sample application and mcubootapp bootloader
   west build -p -b cyw920829m2evk_02 samples/basic/blinky -S mcubootapp

   # Program mcubootapp bootloader
   west build -t mcubootapp-flash

   # Program blinky sample application
   west flash

Description
***********

The module contains a cmake project that is used to generate a build system for building the
MCUBootApp bootloader. Some of the reasons for using this module instead of the normal build
procedure described in `MCUBootApp for the CYW20829 platform`_ could be:

- To use the toolchain from the Zephyr SDK
- To use the ``west build`` default build system, `Ninja`
- No ``cysecuretools`` environment setup required
- No `policy` needs to be specified
- Keys can be specified by configuration options (Kconfig)
- The same keys are automatically selected by the bootloader and the application
- The memory map is automatically generated from the partitions in the device tree

Two new configuration options have been added for CYW20829 devices provisioned for SECURE lifecycle
stage. These options are used to select keys for signing and (optionally) encrypting the MCUBootApp
(L1) image. These keys are independent of the keys used by MCUBootApp to sign and encrypt the
Zephyr application (L2) image.

.. code-block:: kconfig

    config MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE
    	string "Path to the BootROM signature key file"
    	depends on SOC_SERIES = "cyw20829"
    	help
    	  The file contains a key that is used to sign the application image
    	  launched by CYW20829 BootROM. The CYW20829 must have been provisioned
    	  into SECURE lifecycle stage with a matching key.

    config MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE
    	string "Path to the BootROM encryption key file"
    	depends on MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE != ""
    	help
    	  The file contains a raw binary AES-128 key that is used to encrypt the
    	  application image launched by CYW20829 BootROM. The CYW20829 must have
    	  been provisioned into SECURE lifecycle stage with enabled encryption and
    	  the same AES-128 key.

To build a signed and encrypted Zephyr Blinky application (L2) image and a signed and encrypted MCUBootApp
(L1) image for SECURE LCS:

.. code-block:: bash

    west build -p -b cyw920829m2evk_02 samples/basic/blinky -S mcubootapp -- -DCONFIG_MCUBOOTAPP_BOOTROM_SIGNATURE_KEY_FILE=\"/path/to/l1_sig_key.pem\" -DCONFIG_MCUBOOTAPP_BOOTROM_ENCRYPTION_KEY_FILE=\"/path/to/l1_enc_key.bin\" -DCONFIG_MCUBOOT_SIGNATURE_KEY_FILE=\"/path/to/l2_sig_key.pem\" -DCONFIG_MCUBOOT_ENCRYPTION_KEY_FILE=\"/path/to/l2_enc_key.pem\"

Standalone MCUBootApp bootloader
*********************************

The MCUBootApp bootloader can also be built and programmed standalone, independent of any Zephyr
application.

.. code-block:: bash

   cd ~/zephyrproject/zephyr
   west build -p -b cyw920829m2evk_02 ../hakanjansson/zephyr-mcubootapp/app
   west flash

.. _MCUBootApp for the CYW20829 platform:
   https://github.com/mcu-tools/mcuboot/blob/e54c0a3b4f07424732e28f71d9a8153989b2ac48/boot/cypress/platforms/CYW20829.md