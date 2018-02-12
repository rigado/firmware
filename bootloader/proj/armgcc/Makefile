PROJECT_NAME     := rigdfu2
TARGETS          := nrf52832_xxaa
OUTPUT_DIRECTORY := _build

PROJ_DIR := ../..
SDK_VERSION := sdk14
SDK_ROOT := $(PROJ_DIR)/nordicsemi/$(SDK_VERSION)

$(OUTPUT_DIRECTORY)/nrf52832_xxaa.out: \
  LINKER_SCRIPT  := $(PROJ_DIR)/proj/rigdfu_debug_nrf52.ld

# Source files common to all targets
SRC_FILES += \
	$(PROJ_DIR)/src/main.c \
	$(PROJ_DIR)/nordicsemi/other/simple_uart.c \
	$(SDK_ROOT)/components/ble/common/ble_advdata.c \
	$(SDK_ROOT)/components/ble/common/ble_conn_params.c \
	$(SDK_ROOT)/components/ble/common/ble_srv_common.c \
	$(SDK_ROOT)/components/ble/ble_services/ble_dis/ble_dis.c \
	$(SDK_ROOT)/components/drivers_nrf/common/nrf_drv_common.c \
	$(SDK_ROOT)/components/drivers_nrf/ble_flash/ble_flash.c 	\
	$(SDK_ROOT)/components/drivers_nrf/clock/nrf_drv_clock.c \
	$(SDK_ROOT)/components/libraries/util/app_error.c \
	$(SDK_ROOT)/components/libraries/util/app_error_weak.c \
	$(SDK_ROOT)/components/libraries/scheduler/app_scheduler.c \
	$(SDK_ROOT)/components/libraries/timer/app_timer.c \
	$(SDK_ROOT)/components/libraries/util/app_util_platform.c \
	$(SDK_ROOT)/components/libraries/crc16/crc16.c \
	$(SDK_ROOT)/components/libraries/util/nrf_assert.c \
	$(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_app_start.c \
	$(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_mbr.c \
	$(PROJ_DIR)/lib/utils/fstorage.c \
	$(PROJ_DIR)/lib/utils/queue.c \
	$(PROJ_DIR)/lib/utils/crc32.c \
	$(SDK_ROOT)/components/toolchain/system_nrf52.c \
	$(SDK_ROOT)/components/toolchain/gcc/gcc_startup_nrf52.S \
	$(PROJ_DIR)/lib/crypto/modes/ctr/ctr_decrypt.c \
	$(PROJ_DIR)/lib/crypto/modes/ctr/ctr_done.c \
	$(PROJ_DIR)/lib/crypto/modes/ctr/ctr_encrypt.c \
	$(PROJ_DIR)/lib/crypto/modes/ctr/ctr_start.c \
	$(PROJ_DIR)/lib/crypto/encauth/eax/eax_decrypt.c \
	$(PROJ_DIR)/lib/crypto/encauth/eax/eax_done.c \
	$(PROJ_DIR)/lib/crypto/encauth/eax/eax_encrypt.c \
	$(PROJ_DIR)/lib/crypto/encauth/eax/eax_init.c \
	$(PROJ_DIR)/lib/crypto/misc/error_to_string.c \
	$(PROJ_DIR)/lib/crypto/misc/zeromem.c \
	$(PROJ_DIR)/lib/crypto/ltc_nrf.c \
	$(PROJ_DIR)/lib/crypto/mac/omac/omac_done.c \
	$(PROJ_DIR)/lib/crypto/mac/omac/omac_init.c \
	$(PROJ_DIR)/lib/crypto/mac/omac/omac_process.c \
	$(PROJ_DIR)/lib/crypto/test.c \
	$(PROJ_DIR)/nordicsemi/dfu/bootloader.c \
	$(PROJ_DIR)/lib/dfu/dfu_transport_serial.c \
	$(PROJ_DIR)/nordicsemi/dfu/dfu_transport_ble.c \
	$(PROJ_DIR)/nordicsemi/dfu/dfu_dual_bank.c \
	$(PROJ_DIR)/nordicsemi/dfu/ble_dfu.c \
	$(PROJ_DIR)/nordicsemi/dfu/bootloader_settings.c \
	$(PROJ_DIR)/lib/heatshrink/heatshrink_decoder.c \
	$(PROJ_DIR)/nordicsemi/mempool/hci_mem_pool_ble.c \
	$(PROJ_DIR)/lib/patch/bspatch.c \
	$(PROJ_DIR)/lib/patch/patcher.c \
	$(PROJ_DIR)/lib/info/rig_firmware_info.c \
	$(PROJ_DIR)/lib/rigado/rigdfu.c \
	$(PROJ_DIR)/lib/rigado/rigdfu_serial.c \
	$(PROJ_DIR)/lib/rigado/rigdfu_util.c \
	$(SDK_ROOT)/components/libraries/experimental_section_vars/nrf_section_iter.c \
	$(SDK_ROOT)/components/libraries/experimental_memobj/nrf_memobj.c \
	$(SDK_ROOT)/components/libraries/strerror/nrf_strerror.c \
	$(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_backend_rtt.c \
	$(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_backend_serial.c \
	$(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_default_backends.c \
	$(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_str_formatter.c \
	$(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_frontend.c \
	$(SDK_ROOT)/external/fprintf/nrf_fprintf.c \
	$(SDK_ROOT)/external/fprintf/nrf_fprintf_format.c \
	$(SDK_ROOT)/components/softdevice/common/nrf_sdh.c \
	$(SDK_ROOT)/components/softdevice/common/nrf_sdh_soc.c \
	$(SDK_ROOT)/components/softdevice/common/nrf_sdh_ble.c \
	$(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_app_start_asm.c \
	$(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_mbr.c \
	$(SDK_ROOT)/components/libraries/balloc/nrf_balloc.c \
	$(SDK_ROOT)/external/segger_rtt/SEGGER_RTT.c \
	$(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_printf.c \
	$(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_Syscalls_GCC.c \

	#$(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_info.c \

# Include folders common to all targets
INC_FOLDERS += \
	$(PROJ_DIR)/src \
	$(PROJ_DIR)/lib/crypto/headers \
	$(PROJ_DIR)/lib/dfu \
	$(PROJ_DIR)/lib/crypto/ \
	$(PROJ_DIR)/lib/heatshrink/ \
	$(PROJ_DIR)/lib/info \
	$(PROJ_DIR)/lib/patch \
	$(PROJ_DIR)/lib/rigado \
	$(PROJ_DIR)/lib/utils \
	$(PROJ_DIR)/nordicsemi/dfu \
	$(PROJ_DIR)/nordicsemi/mempool \
	$(PROJ_DIR)/nordicsemi/other \
	$(SDK_ROOT)/components/ble/ble_services/ble_dis \
	$(SDK_ROOT)/components/ble/common \
	$(SDK_ROOT)/components/drivers_nrf/common \
	$(SDK_ROOT)/components/drivers_nrf/ble_flash \
	$(SDK_ROOT)/components/drivers_nrf/delay \
	$(SDK_ROOT)/components/drivers_nrf/hal \
	$(SDK_ROOT)/components/libraries/crc16 \
	$(SDK_ROOT)/components/libraries/util \
	$(SDK_ROOT)/components/libraries/scheduler \
	$(SDK_ROOT)/components/libraries/bootloader \
	$(SDK_ROOT)/components/libraries/timer \
	$(SDK_ROOT)/components/device \
	$(SDK_ROOT)/components/softdevice/common \
	$(SDK_ROOT)/components/softdevice/s132/headers \
	$(SDK_ROOT)/components/softdevice/s132/headers/nrf52 \
	$(SDK_ROOT)/components/libraries/bootloader/dfu \
	$(SDK_ROOT)/components/drivers_nrf/clock \
	$(SDK_ROOT)/components/toolchain \
	$(SDK_ROOT)/components/toolchain/cmsis/include \
	$(SDK_ROOT)/components/libraries/experimental_section_vars \
	$(SDK_ROOT)/components/libraries/experimental_log \
	$(SDK_ROOT)/components/libraries/experimental_log\src \
	$(SDK_ROOT)/components/libraries/experimental_memobj/ \
	$(SDK_ROOT)/components/libraries/strerror \
	$(SDK_ROOT)/components/libraries/balloc \
	$(SDK_ROOT)/components/libraries/atomic \
	$(SDK_ROOT)/external/fprintf \
	$(SDK_ROOT)/components/libraries/bootloader/dfu/ \
	$(SDK_ROOT)/external/segger_rtt/ \

# Libraries common to all targets
LIB_FILES += \

# Optimization flags
OPT = -Os -g3
# Uncomment the line below to enable link time optimization
#OPT += -flto

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -DNRF52
CFLAGS += -DSDK14
CFLAGS += -DRELEASE
CFLAGS += -DBLE_STACK_SUPPORT_REQD
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -DS132
CFLAGS += -D__HEAP_SIZE=0
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -Wall
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums 

# C++ flags common to all targets
CXXFLAGS += $(OPT)

# Assembler flags common to all targets
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -DCONFIG_NFCT_PINS_AS_GPIOS
ASMFLAGS += -DNRF52
ASMFLAGS += -DSDK14
ASMFLAGS += -DRELEASE
ASMFLAGS += -DBLE_STACK_SUPPORT_REQD
ASMFLAGS += -DSOFTDEVICE_PRESENT
ASMFLAGS += -DS132
ASMFLAGS += -D__HEAP_SIZE=0

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L $(TEMPLATE_PATH) -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs


# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

.PHONY: default help

# Default target - first one defined
default: nrf52832_xxaa

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo 	nrf52832_xxaa

TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc

include $(TEMPLATE_PATH)/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: flash erase

# Flash the program
flash: $(OUTPUT_DIRECTORY)/nrf52832_xxaa.hex
	@echo Flashing: $<
	nrfjprog -f nrf52 --program $< --sectorerase
	nrfjprog -f nrf52 --reset

erase:
	nrfjprog -f nrf52 --eraseall
