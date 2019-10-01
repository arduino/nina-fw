PROJECT_NAME := nina-fw
M4_PORT := /dev/cu.usbmodem1431201
M4_BOOTLOADER := /Volumes/FEATHERBOOT/.
ESP_PORT := /dev/cu.usbserial-AH03B302

EXTRA_COMPONENT_DIRS := $(PWD)/arduino

ifeq ($(RELEASE),1)
CFLAGS += -DNDEBUG -DCONFIG_FREERTOS_ASSERT_DISABLE -Os -DLOG_LOCAL_LEVEL=ESP_LOG_DEBUG
CPPFLAGS += -DNDEBUG -Os
endif

ifeq ($(UNO_WIFI_REV2),1)
CFLAGS += -DUNO_WIFI_REV2
CPPFLAGS += -DUNO_WIFI_REV2
endif

include $(IDF_PATH)/make/project.mk

# M4 to USB-Serial Passthrough
passthrough:
	cp passthrough.UF2  $(M4_BOOTLOADER)
	echo "Copied to BOOT!"

# Upload to ESP32 after entering passthrough
upload:
	echo "Uploading nina-fw to ESP32"
	esptool.py --port $(M4_PORT) --before no_reset --baud 115200 write_flash 0 NINA_W102-1.3.1.bin

serial:
	miniterm.py -p 

firmware: all
	python combine.py

.PHONY: firmware

.PHONY: passthrough

.PHONY: upload

.PHONY: serial