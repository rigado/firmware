PROJECT_NAME := bmdware

export OUTPUT_FILENAME
#MAKEFILE_NAME := $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
MAKEFILE_NAME := $(MAKEFILE_LIST)
MAKEFILE_DIR := $(dir $(MAKEFILE_NAME) ) 
SDK_ROOT := ../../../nrf5_sdk/
COMMON_ROOT := ../../../common/
TEMPLATE_PATH = $(SDK_ROOT)components/toolchain/gcc

ifeq ($(OS),Windows_NT)
include $(TEMPLATE_PATH)/Makefile.windows
else
include $(TEMPLATE_PATH)/Makefile.posix
endif

MK := mkdir
RM := rm -rf

#echo suspend
ifeq ("$(VERBOSE)","1")
NO_ECHO := 
else
NO_ECHO := @
endif

# Toolchain commands
CC              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-gcc'
AS              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-as'
AR              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ar' -r
LD              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ld'
NM              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-nm'
OBJDUMP         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objdump'
OBJCOPY         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objcopy'
SIZE            := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-size'

#function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))


#source common to all targets
C_SOURCE_FILES += \
$(abspath ../$(dtm_file)) \
$(abspath ../$(selftest_file)) \
$(abspath ../gpio_ctrl_def.c) \
$(abspath ../main.c) \
$(abspath ../storage_intf.c) \
$(abspath $(COMMON_ROOT)/bootloader_info.c) \
$(abspath $(COMMON_ROOT)/crc.c) \
$(abspath $(COMMON_ROOT)/gpio_ctrl.c) \
$(abspath $(COMMON_ROOT)/lock.c) \
$(abspath $(COMMON_ROOT)/rig_firmware_info.c) \
$(abspath $(COMMON_ROOT)/rigdfu_util.c) \
$(abspath $(COMMON_ROOT)/rigdfu.c) \
$(abspath $(COMMON_ROOT)/ringbuf.c) \
$(abspath $(COMMON_ROOT)/service.c) \
$(abspath $(COMMON_ROOT)/settings.c) \
$(abspath $(COMMON_ROOT)/sys_init.c) \
$(abspath $(COMMON_ROOT)/timer.c) \
$(abspath $(COMMON_ROOT)/uart.c) \
$(abspath $(COMMON_ROOT)/at/at_commands_beacon.c) \
$(abspath $(COMMON_ROOT)/at/at_commands_gpio.c) \
$(abspath $(COMMON_ROOT)/at/at_commands_misc.c) \
$(abspath $(COMMON_ROOT)/at/at_commands_uart.c) \
$(abspath $(COMMON_ROOT)/at/at_commands.c) \
$(abspath $(COMMON_ROOT)/at/at_proc.c) \
$(abspath $(COMMON_ROOT)/at/at_utils.c) \
$(abspath $(COMMON_ROOT)/ble/advertising.c) \
$(abspath $(COMMON_ROOT)/ble/ble_beacon_config.c) \
$(abspath $(COMMON_ROOT)/ble/ble_dtm.c) \
$(abspath $(COMMON_ROOT)/ble/ble_nus.c) \
$(abspath $(COMMON_ROOT)/ble/gap.c) \
$(abspath $(COMMON_ROOT)/lib/simple_uart.c) \
$(abspath $(COMMON_ROOT)/timeslot/nrf_advertiser.c) \
$(abspath $(COMMON_ROOT)/timeslot/ts_controller.c) \
$(abspath $(COMMON_ROOT)/timeslot/ts_peripheral.c) \
$(abspath $(COMMON_ROOT)/timeslot/ts_rng.c) \
$(abspath $(SDK_ROOT)components/ble/common/ble_advdata.c) \
$(abspath $(SDK_ROOT)components/ble/common/ble_conn_params.c) \
$(abspath $(SDK_ROOT)components/ble/ble_debug_assert_handler/ble_debug_assert_handler.c) \
$(abspath $(SDK_ROOT)components/ble/ble_services/ble_dis/ble_dis.c) \
$(abspath $(SDK_ROOT)components/ble/ble_error_log/ble_error_log.c) \
$(abspath $(SDK_ROOT)components/ble/common/ble_srv_common.c) \
$(abspath $(SDK_ROOT)components/drivers_nrf/ble_flash/ble_flash.c) \
$(abspath $(SDK_ROOT)components/drivers_nrf/delay/nrf_delay.c) \
$(abspath $(SDK_ROOT)components/drivers_nrf/common/nrf_drv_common.c) \
$(abspath $(SDK_ROOT)components/drivers_nrf/pstorage/pstorage.c) \
$(abspath $(SDK_ROOT)components/libraries/util/app_error.c) \
$(abspath $(SDK_ROOT)components/libraries/util/app_error_weak.c) \
$(abspath $(SDK_ROOT)components/libraries/timer/app_timer.c) \
$(abspath $(SDK_ROOT)components/libraries/util/nrf_assert.c) \
$(abspath $(SDK_ROOT)components/softdevice/common/softdevice_handler/softdevice_handler.c) \
$(abspath $(SDK_ROOT)components/toolchain/$(system_file)) \

#assembly files common to all targets
ASM_SOURCE_FILES += $(abspath $(SDK_ROOT)components/toolchain/gcc/$(startup_file))

#includes common to all targets
INC_PATHS += -I$(abspath ./)
INC_PATHS += -I$(abspath ../)
INC_PATHS += -I$(abspath $(COMMON_ROOT))
INC_PATHS += -I$(abspath $(COMMON_ROOT)/at)
INC_PATHS += -I$(abspath $(COMMON_ROOT)/ble)
INC_PATHS += -I$(abspath $(COMMON_ROOT)/lib)
INC_PATHS += -I$(abspath $(COMMON_ROOT)/timeslot)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/ble/ble_debug_assert_handler)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/ble/ble_error_log)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/ble/ble_services/ble_dis)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/ble/ble_dtm)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/ble/common)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/device)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/drivers_nrf/ble_flash)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/drivers_nrf/common)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/drivers_nrf/config)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/drivers_nrf/delay)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/drivers_nrf/pstorage)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/drivers_nrf/hal)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/libraries/ic_info)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/libraries/timer)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/libraries/util)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/softdevice/common/softdevice_handler)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/softdevice/$(sd_dir)/headers)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/softdevice/$(sd_dir)/headers/$(ic_type))
INC_PATHS += -I$(abspath $(SDK_ROOT)components/toolchain)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/toolchain/CMSIS/Include)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/toolchain/gcc)
INC_PATHS += -I$(abspath $(SDK_ROOT)components/libraries/experimental_section_vars)

OBJECT_DIRECTORY = _build
LISTING_DIRECTORY = $(OBJECT_DIRECTORY)
OUTPUT_BINARY_DIRECTORY = $(OBJECT_DIRECTORY)

# Sorting removes duplicates
BUILD_DIRECTORIES := $(sort $(OBJECT_DIRECTORY) $(OUTPUT_BINARY_DIRECTORY) $(LISTING_DIRECTORY) )

#flags common to all targets
CFLAGS  = -D$(ic_type)
CFLAGS += -D$(softdevice)
CFLAGS += -DSDK11
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -DBLE_STACK_SUPPORT_REQD
ifeq ("$(ic_type)","NRF52")
	CFLAGS += -DAPP_START_ADDRESS=0x1c000
	CFLAGS += -DCONFIG_NFCT_PINS_AS_GPIOS
	CFLAGS += -DNRF52_PAN_12
	CFLAGS += -DNRF52_PAN_15
	CFLAGS += -DNRF52_PAN_58
	CFLAGS += -DNRF52_PAN_55
	CFLAGS += -DNRF52_PAN_54
	CFLAGS += -DNRF52_PAN_31
	CFLAGS += -DNRF52_PAN_30
	CFLAGS += -DNRF52_PAN_51
	CFLAGS += -DNRF52_PAN_36
	CFLAGS += -DNRF52_PAN_53
	CFLAGS += -DNRF52_PAN_20
	CFLAGS += -DNRF52_PAN_64
	CFLAGS += -DNRF52_PAN_62
	CFLAGS += -DNRF52_PAN_63
	CFLAGS += -mcpu=cortex-m4
	CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
else
	ifeq ("$(ic_type)","NRF51")
		CFLAGS += -DAPP_START_ADDRESS=0x1b000
		CFLAGS += -mcpu=cortex-m0
		CFLAGS += -mfloat-abi=soft
	endif
endif

CFLAGS += -mthumb -mabi=aapcs --std=gnu99
CFLAGS += -D__HEAP_SIZE=0

ifeq ("$(bmdware_build_type)", "RELEASE")
	CFLAGS += -Wall -Werror -Os -g0
	CFLAGS += -fomit-frame-pointer
else
	CFLAGS += -Wall -Werror -Og -g3
endif	

# keep every function in separate section. This will allow linker to dump unused functions
CFLAGS += -Wl,--gc-sections -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin --short-enums 

# keep every function in separate section. This will allow linker to dump unused functions
LDFLAGS += -Xlinker -Map=$(LISTING_DIRECTORY)/$(OUTPUT_FILENAME).map
LDFLAGS += -mthumb -mabi=aapcs -L $(TEMPLATE_PATH) -T$(LINKER_SCRIPT)
ifeq ("$(ic_type)","NRF52")
	LDFLAGS += -mcpu=cortex-m4
	LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
else
	ifeq ("$(ic_type)","NRF51")
		LDFLAGS += -mcpu=cortex-m0
		LDFLAGS += -mfloat-abi=soft
	endif
endif
# let linker to dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs -lc -lnosys

# Assembler flags
ASMFLAGS = -x assembler-with-cpp
ASMFLAGS += -D$(ic_type)
ASMFLAGS += -D$(softdevice)
ASMFLAGS += -DSDK11
ASMFLAGS += -DSOFTDEVICE_PRESENT
ASMFLAGS += -DBLE_STACK_SUPPORT_REQD
ifeq ("$(ic_type)","NRF52")
	ASMFLAGS += -DCONFIG_NFCT_PINS_AS_GPIOS
	ASMFLAGS += -DAPP_START_ADDRESS=0x1c000
	ASMFLAGS += -DNRF52_PAN_12
	ASMFLAGS += -DNRF52_PAN_15
	ASMFLAGS += -DNRF52_PAN_58
	ASMFLAGS += -DNRF52_PAN_55
	ASMFLAGS += -DNRF52_PAN_54
	ASMFLAGS += -DNRF52_PAN_31
	ASMFLAGS += -DNRF52_PAN_30
	ASMFLAGS += -DNRF52_PAN_51
	ASMFLAGS += -DNRF52_PAN_36
	ASMFLAGS += -DNRF52_PAN_53
	ASMFLAGS += -DNRF52_PAN_20
	ASMFLAGS += -DNRF52_PAN_64
	ASMFLAGS += -DNRF52_PAN_62
	ASMFLAGS += -DNRF52_PAN_63
else
	ifeq ("$(ic_type)","NRF51")
		ASMFLAGS += -DAPP_START_ADDRESS=0x1b000
	endif
endif
ifeq ("$(bmdware_build_type)","RELEASE")
	ifeq ("$(ic_type)","NRF52")
		TARGET := bmdware_nrf52_s132_rel
	else
		ifeq ("$(ic_type)","NRF51")
			TARGET := bmdware_nrf51_s130_rel
		endif
	endif
else
	ifeq ("$(ic_type)","NRF52")
		TARGET := bmdware_nrf52_s132_dbg
	else
		ifeq ("$(ic_type)","NRF51")
			TARGET := bmdware_nrf51_s130_dbg
		endif
	endif
endif

#default target - first one defined
default: $(TARGET)

#building all targets
all: 
	#$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR)
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e $(TARGET)

#target for printing all targets
help:
	@echo following targets are available:
	@echo 	$(TARGET)
	@echo 	flash_softdevice

C_SOURCE_FILE_NAMES = $(notdir $(C_SOURCE_FILES))
C_PATHS = $(call remduplicates, $(dir $(C_SOURCE_FILES) ) )
C_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(C_SOURCE_FILE_NAMES:.c=.o) )

ASM_SOURCE_FILE_NAMES = $(notdir $(ASM_SOURCE_FILES))
ASM_PATHS = $(call remduplicates, $(dir $(ASM_SOURCE_FILES) ))
ASM_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(ASM_SOURCE_FILE_NAMES:.s=.o) )

vpath %.c $(C_PATHS)
vpath %.s $(ASM_PATHS)

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

$(TARGET): OUTPUT_FILENAME := $(TARGET)
$(TARGET): LINKER_SCRIPT=$(TARGET).ld

$(TARGET): $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -lm -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e finalize

## Create build directories
$(BUILD_DIRECTORIES):
	echo $(MAKEFILE_NAME)
	$(MK) $@

# Create objects from C SRC files
$(OBJECT_DIRECTORY)/%.o: %.c
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) $(CFLAGS) $(INC_PATHS) -c -o $@ $<

# Assemble files
$(OBJECT_DIRECTORY)/%.o: %.s
	@echo Assembly file: $(notdir $<)
	$(NO_ECHO)$(CC) $(ASMFLAGS) $(INC_PATHS) -c -o $@ $<
# Link
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -lm -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
## Create binary .bin file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

finalize: genbin genhex echosize

genbin:
	@echo Preparing: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
genhex: 
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
echosize:
	-@echo ''
	$(NO_ECHO)$(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ''

clean:
	$(RM) $(BUILD_DIRECTORIES)

cleanobj:
	$(RM) $(BUILD_DIRECTORIES)/*.o
flash: $(TARGET)
	@echo Flashing: $(OUTPUT_BINARY_DIRECTORY)/$<.hex
	$(NRFJPROG_ROOT)nrfjprog --program $(OUTPUT_BINARY_DIRECTORY)/$<.hex -f $(ic_type)  --sectorerase
	$(NRFJPROG_ROOT)nrfjprog --reset -f $(ic_type)

## Flash softdevice
flash_softdevice:	
	@echo Flashing: $(softdevice_hex).hex
	$(NRFJPROG_ROOT)nrfjprog --program $(SDK_ROOT)components/softdevice/$(sd_dir)/hex/$(softdevice_hex) -f $(ic_type) --chiperase
	$(NRFJPROG_ROOT)nrfjprog --reset -f $(ic_type)
	
## Recover device
recover:
	@echo Recover device
	$(NRFJPROG_ROOT)nrfjprog -f $(ic_type) --recover