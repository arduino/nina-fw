ifneq ($(filter qemu-prepare qemu-test qemu-test-https,$(MAKECMDGOALS)),)

QEMU_TLS_HOST ?= example.com
QEMU_TLS_PORT ?= 443
QEMU_TLS_PATH ?= /

.PHONY: qemu-prepare qemu-test qemu-test-https

qemu-prepare:
	python3 tools/qemu/qemu_test.py prepare

qemu-test:
	python3 tools/qemu/qemu_test.py test

qemu-test-https:
	@test -n "$(QEMU_WIFI_SSID)" || (echo "QEMU_WIFI_SSID is required"; exit 2)
	python3 tools/qemu/qemu_test.py https --ssid "$(QEMU_WIFI_SSID)" --password "$(QEMU_WIFI_PASSWORD)" --host "$(QEMU_TLS_HOST)" --port "$(QEMU_TLS_PORT)" --path "$(QEMU_TLS_PATH)"

else

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
CFLAGS += -DNDEBUG -DCONFIG_FREERTOS_ASSERT_DISABLE -DLOG_LOCAL_LEVEL=0
CPPFLAGS += -DNDEBUG
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

endif
