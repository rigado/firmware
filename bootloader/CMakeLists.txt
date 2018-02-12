cmake_minimum_required(VERSION 3.5)

set(CMAKE_SYSTEM_NAME Generic)
set(TARGET bootloader)
project(${TARGET} C ASM)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

include(${CMAKE_SOURCE_DIR}/CMake/toolchain_gcc.cmake)

set(CMAKE_C_FLAGS "" CACHE STRING "")
set(CMAKE_CXX_FLAGS "" CACHE STRING "")

set(COMMON_FLAGS "-Og -mcpu=cortex-m4 -mthumb -mthumb-interwork -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffunction-sections -fdata-sections -fno-common -fno-builtin")
set(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=gnu99 -Wall -Wextra -Werror=implicit-function-declaration -Wno-unused-parameter -Wno-missing-field-initializers")

add_definitions("-DNRF52 -D__HEAP_SIZE=0 -DNRF_SD_BLE_API_VERSION=3 -DSDK12 -DBLE_STACK_SUPPORT_REQD -DSOFTDEVICE_PRESENT -DS132")

include_directories(
        ${CMAKE_SOURCE_DIR}/src
        lib/crypto/headers
        lib/crypto
        lib/dfu
        lib/heatshrink
        lib/info
        lib/patch
        lib/rigado
        lib/utils
        nordicsemi/dfu
        nordicsemi/mempool
        nordicsemi/other
        nordicsemi/sdk12/components/ble/ble_debug_assert_handler
        nordicsemi/sdk12/components/ble/ble_error_log
        nordicsemi/sdk12/components/ble/ble_services/ble_dis
        nordicsemi/sdk12/components/ble/common
        nordicsemi/sdk12/components/drivers_nrf/ble_flash
        nordicsemi/sdk12/components/drivers_nrf/common
        nordicsemi/sdk12/components/drivers_nrf/clock
        nordicsemi/sdk12/components/drivers_nrf/delay
        nordicsemi/sdk12/components/drivers_nrf/hal
        nordicsemi/sdk12/components/libraries/bootloader
        nordicsemi/sdk12/components/libraries/bootloader/dfu
        nordicsemi/sdk12/components/libraries/crc16
        nordicsemi/sdk12/components/libraries/ic_info
        nordicsemi/sdk12/components/libraries/util
        nordicsemi/sdk12/components/libraries/scheduler
        nordicsemi/sdk12/components/libraries/timer
        nordicsemi/sdk12/components/device
        nordicsemi/sdk12/components/softdevice/s132/headers
        nordicsemi/sdk12/components/softdevice/s132/headers/nrf52
        nordicsemi/sdk12/components/softdevice/common/softdevice_handler
        nordicsemi/sdk12/components/toolchain
        nordicsemi/sdk12/components/toolchain/cmsis/include
        /usr/local/gcc-arm-none-eabi/arm-none-eabi/include/
)

set(SOURCE_FILES
        lib/crypto/encauth/eax/eax_decrypt.c
        lib/crypto/encauth/eax/eax_done.c
        lib/crypto/encauth/eax/eax_encrypt.c
        lib/crypto/encauth/eax/eax_init.c
        lib/crypto/headers/tomcrypt.h
        lib/crypto/mac/omac/omac_done.c
        lib/crypto/mac/omac/omac_init.c
        lib/crypto/mac/omac/omac_process.c
        lib/crypto/misc/error_to_string.c
        lib/crypto/misc/zeromem.c
        lib/crypto/modes/ctr/ctr_decrypt.c
        lib/crypto/modes/ctr/ctr_done.c
        lib/crypto/modes/ctr/ctr_encrypt.c
        lib/crypto/modes/ctr/ctr_start.c
        lib/crypto/ltc_nrf.c
        lib/dfu/dfu_transport_serial.c
        lib/heatshrink/heatshrink_decoder.c
        lib/info/rig_firmware_info.c
        lib/patch/bspatch.c
        lib/patch/patcher.c
        lib/rigado/rigdfu.c
        lib/rigado/rigdfu_serial.c
        lib/rigado/rigdfu_util.c
        lib/utils/crc32.c
        lib/utils/fstorage.c
        lib/utils/queue.c
        nordicsemi/dfu/ble_dfu.c
        nordicsemi/dfu/bootloader.c
        nordicsemi/dfu/bootloader_settings.c
        nordicsemi/dfu/dfu_dual_bank.c
        nordicsemi/dfu/dfu_transport_ble.c
        nordicsemi/mempool/hci_mem_pool_ble.c
        nordicsemi/other/simple_uart.c
        nordicsemi/sdk12/components/ble/ble_debug_assert_handler/ble_debug_assert_handler.c
        nordicsemi/sdk12/components/ble/ble_services/ble_dis/ble_dis.c
        nordicsemi/sdk12/components/ble/common/ble_advdata.c
        nordicsemi/sdk12/components/ble/common/ble_conn_params.c
        nordicsemi/sdk12/components/ble/common/ble_srv_common.c
        nordicsemi/sdk12/components/drivers_nrf/adc/nrf_drv_adc.c
        nordicsemi/sdk12/components/drivers_nrf/ble_flash/ble_flash.c
        nordicsemi/sdk12/components/drivers_nrf/clock/nrf_drv_clock.c
        nordicsemi/sdk12/components/drivers_nrf/common/nrf_drv_common.c
        nordicsemi/sdk12/components/libraries/bootloader/dfu/nrf_dfu_mbr.c
        nordicsemi/sdk12/components/libraries/bootloader/nrf_bootloader_app_start.c
        nordicsemi/sdk12/components/libraries/bootloader/nrf_bootloader_info.c
        nordicsemi/sdk12/components/libraries/crc16/crc16.c
        nordicsemi/sdk12/components/libraries/crc32/crc32.c
        nordicsemi/sdk12/components/libraries/ic_info/nrf_ic_info.c
        nordicsemi/sdk12/components/libraries/scheduler/app_scheduler.c
        nordicsemi/sdk12/components/libraries/timer/app_timer.c
        nordicsemi/sdk12/components/libraries/timer/app_timer.h
        nordicsemi/sdk12/components/libraries/util/app_error.c
        nordicsemi/sdk12/components/libraries/util/app_error_weak.c
        nordicsemi/sdk12/components/libraries/util/app_util_platform.c
        nordicsemi/sdk12/components/libraries/util/nrf_assert.c
        nordicsemi/sdk12/components/softdevice/common/softdevice_handler/softdevice_handler.c
        nordicsemi/sdk12/components/toolchain/system_nrf52.c
#        nordicsemi/sdk12/external/segger_rtt/RTT_Syscalls_GCC.c
#        nordicsemi/sdk12/external/segger_rtt/RTT_Syscalls_IAR.c
#        nordicsemi/sdk12/external/segger_rtt/RTT_Syscalls_KEIL.c
#        nordicsemi/sdk12/external/segger_rtt/SEGGER_RTT.c
#        nordicsemi/sdk12/external/segger_rtt/SEGGER_RTT.h
#        nordicsemi/sdk12/external/segger_rtt/SEGGER_RTT_Conf.h
#        nordicsemi/sdk12/external/segger_rtt/SEGGER_RTT_printf.c
#        nordicsemi/segger_rtt/RTT_Syscalls_GCC.c
#        nordicsemi/segger_rtt/RTT_Syscalls_IAR.c
#        nordicsemi/segger_rtt/RTT_Syscalls_KEIL.c
#        nordicsemi/segger_rtt/SEGGER_RTT.c
#        nordicsemi/segger_rtt/SEGGER_RTT.h
#        nordicsemi/segger_rtt/SEGGER_RTT_Conf.h
#        nordicsemi/segger_rtt/SEGGER_RTT_printf.c
         nordicsemi/sdk12/components/toolchain/gcc/gcc_startup_nrf52.S
        nordicsemi/sdk12/components/toolchain/system_nrf52.c
        src/main.c)

set_source_files_properties(nordicsemi/sdk12/components/toolchain/gcc/gcc_startup_nrf52.S "-x assembler-with-cpp")
set_source_files_properties(nordicsemi/sdk12/components/libraries/bootloader/nrf_bootloader_app_start.c "-fomit-frame-pointer")

add_executable(${TARGET} ${SOURCE_FILES})


set(MY_LINK_FLAGS "-Xlinker -Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
        "-L${CMAKE_SOURCE_DIR}/nordicsemi/sdk12/components/toolchain/gcc -T${CMAKE_SOURCE_DIR}/proj/rigdfu_debug_nrf52.ld"
        "-Wl,--gc-sections -mthumb -mcpu=cortex-m4 -mabi=aapcs -mfloat-abi=hard"
        "-mfpu=fpv4-sp-d16 --specs=nano.specs -lc -lnosys")
string(REGEX REPLACE ";" " " MY_LINK_FLAGS "${MY_LINK_FLAGS}")

set(CMAKE_EXE_LINKER_FLAGS ${MY_LINK_FLAGS})

create_hex(${TARGET})
create_bin(${TARGET})

