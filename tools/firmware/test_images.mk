FIRMWARE_TEST_SOURCES := $(wildcard $(CURDIR)/tests/qemu/*/sketch.ino.cpp)
FIRMWARE_TEST_NAMES := $(notdir $(patsubst %/,%,$(dir $(FIRMWARE_TEST_SOURCES))))
FIRMWARE_TEST_ROOT := $(BUILD_DIR_BASE)/qemu-tests
FIRMWARE_TEST_CRT_BUNDLE := $(CURDIR)/x509_crt_bundle
FIRMWARE_COMPONENT_ARCHIVES := $(foreach component,$(COMPONENT_LIBRARIES),$(BUILD_DIR_BASE)/$(component)/lib$(component).a)
FIRMWARE_TEST_FLASHES := $(foreach test,$(FIRMWARE_TEST_NAMES),$(FIRMWARE_TEST_ROOT)/$(test)/$(test).flash.bin)

define GenerateFirmwareTest
FIRMWARE_TEST_$(1)_SOURCE := $(CURDIR)/tests/qemu/$(1)/sketch.ino.cpp
FIRMWARE_TEST_$(1)_DIR := $(FIRMWARE_TEST_ROOT)/$(1)
FIRMWARE_TEST_$(1)_OBJECT := $$(FIRMWARE_TEST_$(1)_DIR)/sketch.ino.o
FIRMWARE_TEST_$(1)_ELF := $$(FIRMWARE_TEST_$(1)_DIR)/$(1).elf
FIRMWARE_TEST_$(1)_MAP := $$(FIRMWARE_TEST_$(1)_DIR)/$(1).map
FIRMWARE_TEST_$(1)_BIN := $$(FIRMWARE_TEST_$(1)_DIR)/$(1).bin
FIRMWARE_TEST_$(1)_FLASH := $$(FIRMWARE_TEST_$(1)_DIR)/$(1).flash.bin

-include $$(FIRMWARE_TEST_$(1)_OBJECT:.o=.d)

$$(FIRMWARE_TEST_$(1)_OBJECT): $$(FIRMWARE_TEST_$(1)_SOURCE) $(COMPONENT_PROJECT_VARS)
	@mkdir -p $$(dir $$@)
	$$(summary) CXX $$(patsubst $$(PWD)/%,%,$$@)
	$$(CXX) $$(CXXFLAGS) $$(CPPFLAGS) $$(addprefix -I ,$$(COMPONENT_INCLUDES)) -I $$(CURDIR)/main -I $$(dir $$<) -c $$< -o $$@

$$(FIRMWARE_TEST_$(1)_ELF): $$(FIRMWARE_TEST_$(1)_OBJECT) $(FIRMWARE_COMPONENT_ARCHIVES) $(COMPONENT_LINKER_DEPS)
	$$(summary) LD $$(patsubst $$(PWD)/%,%,$$@)
	$$(CC) $$(FIRMWARE_TEST_$(1)_OBJECT) $$(LDFLAGS) -o $$@ -Wl,-Map=$$(FIRMWARE_TEST_$(1)_MAP)

$$(FIRMWARE_TEST_$(1)_BIN): $$(FIRMWARE_TEST_$(1)_ELF) $(ESPTOOLPY_SRC) | check_python_dependencies
	$$(summary) BIN $$(patsubst $$(PWD)/%,%,$$@)
	$$(ESPTOOLPY) elf2image $$(ESPTOOL_FLASH_OPTIONS) $$(ESPTOOL_ELF2IMAGE_OPTIONS) -o $$@ $$<

$$(FIRMWARE_TEST_$(1)_FLASH): $$(FIRMWARE_TEST_$(1)_BIN) $(CURDIR)/data/phy.bin $(FIRMWARE_TEST_CRT_BUNDLE) $(CURDIR)/data/spiffs.bin | $(BUILD_DIR_BASE)/bootloader/bootloader.bin $(BUILD_DIR_BASE)/partitions.bin
	$$(summary) MERGE $$(patsubst $$(PWD)/%,%,$$@)
	$$(ESPTOOLPY) merge_bin --fill-flash-size 4MB -o $$@ \
		0x1000 $(BUILD_DIR_BASE)/bootloader/bootloader.bin \
		0x8000 $(BUILD_DIR_BASE)/partitions.bin \
		0x9000 $(CURDIR)/data/phy.bin \
		0xA000 $(FIRMWARE_TEST_CRT_BUNDLE) \
		0x30000 $$(FIRMWARE_TEST_$(1)_BIN) \
		0x1B0000 $(CURDIR)/data/spiffs.bin
endef

$(foreach test,$(FIRMWARE_TEST_NAMES),$(eval $(call GenerateFirmwareTest,$(test))))

.PHONY: firmware-test-images

ifeq ($(strip $(FIRMWARE_TEST_NAMES)),)
firmware-test-images:
	@echo "error: no firmware tests found under tests/qemu" >&2
	@false
else
firmware-test-images: $(FIRMWARE_TEST_FLASHES)
endif
