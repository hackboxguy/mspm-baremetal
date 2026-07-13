BUILD_DIR := build/$(BOARD)_$(APP)_$(VARIANT)
OUT_DIR := output/$(BOARD)/$(APP)/$(VARIANT)
TARGET := $(BOARD)_$(APP)

ELF := $(OUT_DIR)/$(TARGET).elf
BIN := $(OUT_DIR)/$(TARGET).bin
HEX := $(OUT_DIR)/$(TARGET).hex
MAP := $(OUT_DIR)/$(TARGET).map

PYTHON ?= python3
IMAGE_IDENTITY_TOOL := $(PROJ_ROOT)/tools/stamp_image_identity.py
SOURCE_ID ?= $(shell git rev-parse --verify --short=12 HEAD 2>/dev/null || printf unknown)
SOURCE_DIRTY ?= $(shell if test -n "$$(git status --porcelain --untracked-files=normal 2>/dev/null)"; then printf 1; else printf 0; fi)

ifeq ($(DEBUG),on)
IMAGE_IDENTITY_DEBUG_ARG := --debug
else
IMAGE_IDENTITY_DEBUG_ARG :=
endif

IMAGE_IDENTITY_LAYOUT_ARGS := --flash-origin $(FLASH_ORIGIN) --flash-length $(FLASH_LENGTH)

APP_SRCS := $(wildcard app/$(APP)/*.c)
BOARD_SRCS := $(wildcard board/$(BOARD)/*.c)
HAL_SRCS := $(wildcard hal/*.c)
LIB_SRCS := $(wildcard lib/*.c)
DEVICE_SRCS := $(DEVICE_DIR)/startup.c $(DEVICE_DIR)/image_identity.c
C_SRCS := $(APP_SRCS) $(BOARD_SRCS) $(HAL_SRCS) $(LIB_SRCS) $(DEVICE_SRCS)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o))
DEPS := $(OBJECTS:.o=.d)

FORMAT_SOURCES := $(wildcard app/*/*.c) $(wildcard app/*/*.h) \
	$(wildcard board/*/*.c) $(wildcard board/*/*.h) \
	$(wildcard device/*/*.c) $(wildcard device/*/*.h) \
	$(wildcard hal/*.c) $(wildcard hal/*.h) $(wildcard lib/*.c) \
	$(wildcard lib/*.h) $(wildcard tests/*.c)

CPPFLAGS := -I$(PROJ_ROOT)/lib $(DEVICE_CPPFLAGS) $(VARIANT_CPPFLAGS)
CFLAGS := $(COMMON_CFLAGS) $(CPU_FLAGS) $(OPTIMIZATION)
LDFLAGS := $(CPU_FLAGS) -nostdlib -nostartfiles -nodefaultlibs \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,-Map,$(MAP) \
	-Wl,--cref -T$(LINKER_SCRIPT)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(OUT_DIR):
	@mkdir -p $@

.PHONY: image-identity-force

image-identity-force:

$(ELF): image-identity-force $(OBJECTS) $(LINKER_SCRIPT) $(IMAGE_IDENTITY_TOOL) | $(OUT_DIR)
	$(CC) $(LDFLAGS) $(OBJECTS) -lgcc -o $@
	$(PYTHON) $(IMAGE_IDENTITY_TOOL) stamp --elf $@ --version $(VERSION) \
		--source-id "$(SOURCE_ID)" --source-dirty $(SOURCE_DIRTY) \
		$(IMAGE_IDENTITY_DEBUG_ARG) $(IMAGE_IDENTITY_LAYOUT_ARGS)
	$(PYTHON) $(IMAGE_IDENTITY_TOOL) verify --elf $@ $(IMAGE_IDENTITY_LAYOUT_ARGS)

$(BIN): $(ELF)
	$(OBJCOPY) --gap-fill 0xff -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

.PHONY: identity-check

identity-check: $(ELF) $(BIN) $(HEX)
	$(PYTHON) $(IMAGE_IDENTITY_TOOL) verify --elf $(ELF) --bin $(BIN) --hex $(HEX) \
		$(IMAGE_IDENTITY_LAYOUT_ARGS)

identity-readback: $(ELF)
	OPENOCD="$(OPENOCD)" PYTHON="$(PYTHON)" \
		sh tools/openocd/read_image_identity_lp_mspm0c1106.sh $(ELF)

-include $(DEPS)
