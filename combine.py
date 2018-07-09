#!/usr/bin/env python

booloaderData = open("build/bootloader/bootloader.bin", "rb").read()
partitionData = open("build/partitions_singleapp.bin", "rb").read()
appData = open("build/mkrwifi1010-fw.bin", "rb").read()

# calculate the output binary size, app offset 
outputSize = 0x10000 + len(appData)
if (outputSize % 1024):
	outputSize += 1024 - (outputSize % 1024)

# allocate and init to 0xff
outputData = bytearray(b'\xff') * outputSize

# copy data: bootloader, partitions, app
for i in range(0, len(booloaderData)):
	outputData[0x1000 + i] = booloaderData[i]

for i in range(0, len(partitionData)):
	outputData[0x8000 + i] = partitionData[i]

for i in range(0, len(appData)):
	outputData[0x10000 + i] = appData[i]

# write out
with open("NINA_W102.bin","w+b") as f:
	f.seek(0)
	f.write(outputData)
