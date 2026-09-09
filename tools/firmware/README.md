# Containerized firmware build

Build the current checkout with the pinned ESP-IDF container image:

```sh
make firmware-container
```

The target automatically detects a running Apple `container`, Docker, or
Podman engine. It builds directly in the checkout so the ignored `build/`
directory is reused on subsequent invocations.

Configuration is available through Make variables:

```sh
make firmware-container \
  FIRMWARE_CONTAINER_ENGINE=docker \
  FIRMWARE_JOBS=2 \
  FIRMWARE_IMAGE=espressif/idf:v4.4.8
```

By default, Make uses the CPU count visible inside the container. Limit
`FIRMWARE_JOBS` when the container has constrained memory.
