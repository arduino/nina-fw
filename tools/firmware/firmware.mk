FIRMWARE_IMAGE ?= espressif/idf:v4.4.8
FIRMWARE_ENV = FIRMWARE_IMAGE="$(FIRMWARE_IMAGE)" $(if $(FIRMWARE_CONTAINER_ENGINE),FIRMWARE_CONTAINER_ENGINE="$(FIRMWARE_CONTAINER_ENGINE)") $(if $(FIRMWARE_JOBS),FIRMWARE_JOBS="$(FIRMWARE_JOBS)")

.PHONY: firmware-container firmware-test-images-container firmware-clean

firmware-container:
	$(FIRMWARE_ENV) sh tools/firmware/build_container.sh "$(CURDIR)"

firmware-test-images-container:
	$(FIRMWARE_ENV) sh tools/firmware/build_container.sh --target firmware-test-images "$(CURDIR)"

firmware-clean:
	sh tools/firmware/clean.sh
