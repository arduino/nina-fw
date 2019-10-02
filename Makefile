PROJECT_NAME := nina-fw

# Ports
M4_PORT := /dev/cu.usbmodem141224241
ESP_PORT := /dev/cu.usbserial-AH03B302

# Directories and Files
BOOT_VOLUME := /Volumes/FEATHERBOOT/.
CIRCUITPYTHON_UF2 := adafruit-circuitpython-feather_m4_express-en_US-4.1.0.uf2

UPLOAD_BAUD = 115200
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

load-passthrough:
	cp passthrough.UF2  $(BOOT_VOLUME)

load-nina:
	esptool.py --port $(M4_PORT) --before no_reset --baud $(UPLOAD_BAUD) write_flash 0 NINA_W102-1.3.1.bin

load-circuitpython:
	cp $(CIRCUITPYTHON_UF2) $(BOOT_VOLUME)

serial:
	miniterm.py $(ESP_PORT) $(UPLOAD_BAUD)

firmware: all
	python combine.py

.PHONY: firmware

.PHONY: load-passthrough

.PHONY: load-nina

.PHONY: load-circuitpython

.PHONY: serial