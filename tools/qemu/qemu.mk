QEMU_ROOT ?= $(CURDIR)/build/qemu
CALIB_REPOSITORY ?= https://github.com/Ebiroll/calib.git
CALIB_REF ?= 73bee9bab5a78b0aaa584610b1564d533e7879ce
QEMU_BINARY ?= $(QEMU_ROOT)/build/qemu-system-xtensa
QEMU_BIOS ?= $(QEMU_ROOT)/source/pc-bios
QEMU_RUN_TIMEOUT ?= 600

QEMU_ENV = QEMU_ROOT="$(QEMU_ROOT)" CALIB_REPOSITORY="$(CALIB_REPOSITORY)" CALIB_REF="$(CALIB_REF)" $(if $(QEMU_JOBS),QEMU_JOBS="$(QEMU_JOBS)") $(FIRMWARE_ENV)
QEMU_ARGS = --qemu "$(QEMU_BINARY)" --bios "$(QEMU_BIOS)" --run-timeout "$(QEMU_RUN_TIMEOUT)"

.PHONY: qemu-prepare qemu-test qemu-clean

qemu-prepare:
	$(QEMU_ENV) sh tools/qemu/prepare.sh
	$(QEMU_ENV) python3 tools/qemu/qemu_test.py prepare --qemu "$(QEMU_BINARY)" --bios "$(QEMU_BIOS)"

qemu-test: qemu-prepare firmware-test-images-container
	$(QEMU_ENV) python3 tools/qemu/qemu_test.py test $(QEMU_ARGS)

qemu-clean:
	python3 tools/qemu/qemu_test.py clean
