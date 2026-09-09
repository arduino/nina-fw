QEMU_GOALS := qemu-prepare qemu-test qemu-test-https qemu-clean
TOOLING_GOALS := firmware-container firmware-clean $(QEMU_GOALS)

ifneq ($(filter $(TOOLING_GOALS),$(MAKECMDGOALS)),)

include tools/firmware/firmware.mk
ifneq ($(filter $(QEMU_GOALS),$(MAKECMDGOALS)),)
include tools/qemu/qemu.mk
endif

else

.DEFAULT_GOAL := all

PROJECT_NAME := nina-fw

EXTRA_COMPONENT_DIRS := $(CURDIR)/arduino

CRT_BUNDLE := $(CURDIR)/x509_crt_bundle
CRT_BUNDLE_CONFIG := $(CURDIR)/.x509_crt_bundle.mk
CRT_BUNDLE_INPUT := $(CURDIR)/data/cacrt_all.pem
CRT_BUNDLE_GENERATOR := $(CURDIR)/tools/gen_crt_bundle.py

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(CRT_BUNDLE_CONFIG)
$(info SIZEIS $(BUNDLE_SIZE))

$(CRT_BUNDLE): $(CRT_BUNDLE_INPUT) $(CRT_BUNDLE_GENERATOR)
	cd "$(CURDIR)" && python "$(CRT_BUNDLE_GENERATOR)" -i "$(CRT_BUNDLE_INPUT)"

$(CRT_BUNDLE_CONFIG): $(CRT_BUNDLE)
	@printf 'BUNDLE_SIZE := %s\n' "$$(wc -c < "$(CRT_BUNDLE)")" > "$@"

CPPFLAGS += -DARDUINO -DCRT_BUNDLE_SIZE=$(BUNDLE_SIZE)
endif


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
	rm -rf .x509_crt_bundle.mk

.PHONY: firmware, clean

endif
