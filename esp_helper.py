import time
import os
import subprocess
import argparse

parser = argparse.ArgumentParser()

parser.add_argument('-p', action='store', dest='port_num',
                    help='Board port name')
parser.add_argument('-uf2', action='store', dest='uf2',
                    help='.UF2 file')

results = parser.parse_args()
dest_port = results.port_num
uf2_file = results.uf2

def search_ports():
	proc = subprocess.Popen(["ls /dev/cu.* | grep cu.usbmodem"], stdout=subprocess.PIPE, shell=True)
	(out, err) = proc.communicate()
	avail_ports = out.decode('utf-8')
	return avail_ports

# Scan all ports for FEATHERBOOT
avail_ports = search_ports()
while not avail_ports:
	search_ports()
	print("Board not found, double-tap RESET to enter the bootloader...")
	time.sleep(1)

# cp provided uf2 to featherboot

# wait for featherboot to pop back up as a usbmodem

# execute esptool on port with compiled fw, the start up miniterm.py

