BUILD_DIR := build/$(BOARD)_$(APP)_$(VARIANT)
OUT_DIR := output/$(BOARD)/$(APP)/$(VARIANT)
TARGET := $(BOARD)_$(APP)

ELF := $(OUT_DIR)/$(TARGET).elf
BIN := $(OUT_DIR)/$(TARGET).bin
HEX := $(OUT_DIR)/$(TARGET).hex
MAP := $(OUT_DIR)/$(TARGET).map

APP_SRCS := $(wildcard app/$(APP)/*.c)
BOARD_SRCS := $(wildcard board/$(BOARD)/*.c)
HAL_SRCS := $(wildcard hal/*.c)
DEVICE_SRCS := $(DEVICE_DIR)/startup.c $(DEVICE_DIR)/image_identity_placeholder.c
C_SRCS := $(APP_SRCS) $(BOARD_SRCS) $(HAL_SRCS) $(DEVICE_SRCS)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o))
DEPS := $(OBJECTS:.o=.d)

FORMAT_SOURCES := $(C_SRCS) $(wildcard board/$(BOARD)/*.h) \
	$(wildcard $(DEVICE_DIR)/*.h) $(wildcard hal/*.h) $(wildcard tests/*.c)

CPPFLAGS := $(DEVICE_CPPFLAGS) $(VARIANT_CPPFLAGS)
CFLAGS := $(COMMON_CFLAGS) $(CPU_FLAGS) $(OPTIMIZATION)
LDFLAGS := $(CPU_FLAGS) -nostdlib -nostartfiles -nodefaultlibs \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,-Map,$(MAP) \
	-Wl,--cref -T$(LINKER_SCRIPT)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(OUT_DIR):
	@mkdir -p $@

$(ELF): $(OBJECTS) $(LINKER_SCRIPT) | $(OUT_DIR)
	$(CC) $(LDFLAGS) $(OBJECTS) -lgcc -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

-include $(DEPS)
