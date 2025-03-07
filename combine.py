#!/usr/bin/env python

import sys;

booloaderData = open("build/bootloader/bootloader.bin", "rb").read()
partitionData = open("build/partitions.bin", "rb").read()
phyData = open("data/phy.bin", "rb").read()
certsData = open("data/roots.pem", "rb").read()
appData = open("build/nina-fw.bin", "rb").read()

# Offset   Size     Name
# 0x001000 0x007000 bootloader
# 0x008000 0x001000 partitions
# 0x009000 0x001000 phy data
# 0x00A000 0x026000 certs
# 0x030000 0x180000 app
# 0x1B0000 0x040000 spiffs
# 0x1F0000 0x010000 nvs

# calculate the output binary size, app offset 
outputSize = 0x30000 + len(appData)
if (outputSize % 1024):
	outputSize += 1024 - (outputSize % 1024)

# allocate and init to 0xff
outputData = bytearray(b'\xff') * outputSize

# copy data: bootloader, partitions, app
for i in range(0, len(booloaderData)):
	outputData[0x1000 + i] = booloaderData[i]

for i in range(0, len(partitionData)):
	outputData[0x8000 + i] = partitionData[i]

for i in range(0, len(phyData)):
    outputData[0x9000 + i] = phyData[i]

for i in range(0, len(certsData)):
    outputData[0xA000 + i] = certsData[i]

# zero terminate the pem file
outputData[0xA000 + len(certsData)] = 0

for i in range(0, len(appData)):
	outputData[0x30000 + i] = appData[i]


outputFilename = "NINA_W102.bin"
if (len(sys.argv) > 1):
	outputFilename = sys.argv[1]

# write out
with open(outputFilename,"w+b") as f:
	f.seek(0)
	f.write(outputData)
