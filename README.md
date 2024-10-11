# Arduino NINA-W102 firmware

This firmware uses [Espressif's IDF](https://github.com/espressif/esp-idf)

## Building

1. [Download the ESP32 toolchain](http://esp-idf.readthedocs.io/en/v3.3.1/get-started/index.html#setup-toolchain)
1. Extract it and add it to your `PATH`: `export PATH=$PATH:<path/to/toolchain>/bin`
1. Clone **v3.3.1** of the IDF: `git clone --branch v3.3.1 --recursive https://github.com/espressif/esp-idf.git`
1. Set the `IDF_PATH` environment variable: `export IDF_PATH=<path/to/idf>`
1. Run `make` to build the firmware (in the directory of this read me)
1. Load the `Tools -> SerialNINAPassthrough` example sketch on to the board
1. Use `esptool` to flash the compiled firmware

### Building with docker

As an alternative for building we can use the docker image from espressif idf, we can do that as follows:

```
docker run -v $PWD:/data espressif/idf:v3.3.1 -- sh -c 'cd /data && make'
```

You can also flash the firmware with the following snippet:

```
DEVICE=/dev/ttyACM0
docker run --device=$DEVICE -v $PWD:/data espressif/idf:v3.3.1 -- sh -c 'cd /data && make flash ESPPORT=$DEVICE'
```

## Notes
If updating the NINA firmware for an **Arduino UNO WiFi Rev. 2** or **Arduino Nano RP2040** board via [SerialNINAPassthrough](https://github.com/arduino-libraries/WiFiNINA/blob/master/examples/Tools/SerialNINAPassthrough/SerialNINAPassthrough.ino) sketch, then the `esptool` invocation needs to be changed slightly:
```diff
-  --baud 115200 --before default_reset
+  --baud 115200 --before no_reset
```

## Packaging
The `make` command produces a bunch of binary files that must be flashed at very precise locations, making `esptool` commandline quite complicated.
Instead, once the firmware has been compiled, you can invoke `combine.py` script to produce a monolithic binary that can be flashed at 0x0.
```
make
python combine.py
```
This produces `NINA_W102.bin` file (a different name can be specified as parameter). To flash this file you can use https://arduino.github.io/arduino-fwuploader/2.2/usage/#firmware-flashing

## Build a new certificate list (based on the Google Android root CA list)
```bash
git clone https://android.googlesource.com/platform/system/ca-certificates
cp nina-fw/tools/nina-fw-create-roots.sh ca-certificates/files
cd ca-certificates/files
./nina-fw-create-roots.sh
cp roots.pem ../../nina-fw/data/roots.pem
```

## Check certificate list against URL list
```bash
cd tools
./sslcheck.sh -c ../data/roots.pem -l url_lists/url_list_moz.com.txt -e
```

## License

Copyright (c) 2018-2019 Arduino SA. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
