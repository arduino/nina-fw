PROJECT_NAME := nina-fw

# Ports
M4_PORT := /dev/cu.usbmodem1431201
ESP_PORT := /dev/cu.usbserial-AH03B302

# Directories and Files
BOOT_VOLUME := /Volumes/FEATHERBOOT/.
CIRCUITPYTHON_UF2 := adafruit-circuitpython-feather_m4_express-en_US-4.1.0.uf2

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

passthrough:
	cp passthrough.UF2  $(BOOT_VOLUME)

upload-nina:
	esptool.py --port $(M4_PORT) --before no_reset --baud 115200 write_flash 0 NINA_W102-1.3.1.bin

upload-circuitpython:
	cp $(CIRCUITPYTHON_UF2) $(BOOT_VOLUME)

serial:
	miniterm.py $(ESP_PORT) 115200

firmware: all
	python combine.py

.PHONY: firmware

.PHONY: passthrough

.PHONY: upload-nina

.PHONY: upload-circuitpython

.PHONY: serial