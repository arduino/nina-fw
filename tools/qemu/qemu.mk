QEMU_ROOT ?= $(CURDIR)/build/qemu
CALIB_REPOSITORY ?= https://github.com/Ebiroll/calib.git
CALIB_REF ?= 73bee9bab5a78b0aaa584610b1564d533e7879ce
QEMU_BINARY ?= $(QEMU_ROOT)/build/qemu-system-xtensa
QEMU_BIOS ?= $(QEMU_ROOT)/source/pc-bios
QEMU_PATCHED_REF ?= HEAD
QEMU_TLS_HOST ?= example.com
QEMU_TLS_PORT ?= 443
QEMU_TLS_PATH ?= /

QEMU_ENV = QEMU_ROOT="$(QEMU_ROOT)" CALIB_REPOSITORY="$(CALIB_REPOSITORY)" CALIB_REF="$(CALIB_REF)" $(if $(QEMU_JOBS),QEMU_JOBS="$(QEMU_JOBS)") $(FIRMWARE_ENV)
QEMU_ARGS = --qemu "$(QEMU_BINARY)" --bios "$(QEMU_BIOS)" --patched-ref "$(QEMU_PATCHED_REF)"
QEMU_TEST_ARGS = $(QEMU_ARGS) $(if $(QEMU_BASELINE_REF),--baseline-ref "$(QEMU_BASELINE_REF)")

.PHONY: qemu-prepare qemu-test qemu-test-https qemu-clean

qemu-prepare:
	$(QEMU_ENV) sh tools/qemu/prepare.sh
	$(QEMU_ENV) python3 tools/qemu/qemu_test.py prepare --qemu "$(QEMU_BINARY)" --bios "$(QEMU_BIOS)"

qemu-test: qemu-prepare
	$(QEMU_ENV) python3 tools/qemu/qemu_test.py test $(QEMU_TEST_ARGS)

qemu-test-https: qemu-prepare
	@test -n "$(QEMU_WIFI_SSID)" || (echo "QEMU_WIFI_SSID is required"; exit 2)
	$(QEMU_ENV) python3 tools/qemu/qemu_test.py https $(QEMU_ARGS) --ssid "$(QEMU_WIFI_SSID)" --password "$(QEMU_WIFI_PASSWORD)" --host "$(QEMU_TLS_HOST)" --port "$(QEMU_TLS_PORT)" --path "$(QEMU_TLS_PATH)"

qemu-clean:
	python3 tools/qemu/qemu_test.py clean
