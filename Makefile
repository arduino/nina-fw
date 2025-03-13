PROJECT_NAME := nina-fw

EXTRA_COMPONENT_DIRS := $(PWD)/arduino

CMD_GEN_BUNDLE = $(shell python $(PWD)/tools/gen_crt_bundle.py -i $(PWD)/data/cacrt_all.pem)
CMD_BUNDLE_SIZE = $(shell stat -L -c %s ./x509_crt_bundle)
READ_BUNDLE_SIZE = $(eval BUNDLE_SIZE=$(CMD_BUNDLE_SIZE))

$(CMD_GEN_BUNDLE)
$(READ_BUNDLE_SIZE)
$(info SIZEIS $(BUNDLE_SIZE))

CPPFLAGS += -DARDUINO -DCRT_BUNDLE_SIZE=$(BUNDLE_SIZE)

ifeq ($(RELEASE),1)
CFLAGS += -DNDEBUG -DCONFIG_FREERTOS_ASSERT_DISABLE -Os -DLOG_LOCAL_LEVEL=0
CPPFLAGS += -DNDEBUG -Os
$(info RELEASE)
endif

ifeq ($(UNO_WIFI_REV2),1)
CFLAGS += -DUNO_WIFI_REV2
CPPFLAGS += -DUNO_WIFI_REV2
$(info UNO_WIFI_REV2)
endif

ifeq ($(NANO_RP2040_CONNECT),1)
CFLAGS += -DNANO_RP2040_CONNECT
CPPFLAGS += -DNANO_RP2040_CONNECT
$(info NANO_RP2040_CONNECT)
endif

include $(IDF_PATH)/make/project.mk

firmware: all
	python combine.py

clean:
	rm -rf NINA_W102*
	rm -rf x509_crt_bundle

.PHONY: firmware, clean
