#!/usr/bin/env python2
#
# ESP32 partition table generation tool
#
# Converts partition tables to/from CSV and binary formats.
#
# See http://esp-idf.readthedocs.io/en/latest/api-guides/partition-tables.html
# for explanation of partition table structure and uses.
#
# Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from __future__ import print_function, division
import argparse
import os
import re
import struct
import sys

MAX_PARTITION_LENGTH = 0xC00   # 3K for partition data (96 entries) leaves 1K in a 4K sector for signature

__version__ = '1.0'

quiet = False

def status(msg):
    """ Print status message to stderr """
    if not quiet:
        critical(msg)

def critical(msg):
    """ Print critical message to stderr """
    if not quiet:
        sys.stderr.write(msg)
        sys.stderr.write('\n')

class PartitionTable(list):
    def __init__(self):
        super(PartitionTable, self).__init__(self)

    @classmethod
    def from_csv(cls, csv_contents):
        res = PartitionTable()
        lines = csv_contents.splitlines()

        def expand_vars(f):
            f = os.path.expandvars(f)
            m = re.match(r'(?<!\\)\$([A-Za-z_][A-Za-z0-9_]*)', f)
            if m:
                raise InputError("unknown variable '%s'" % m.group(1))
            return f

        for line_no in range(len(lines)):
            line = expand_vars(lines[line_no]).strip()
            if line.startswith("#") or len(line) == 0:
                continue
            try:
                res.append(PartitionDefinition.from_csv(line))
            except InputError as e:
                raise InputError("Error at line %d: %s" % (line_no+1, e))
            except Exception:
                critical("Unexpected error parsing line %d: %s" % (line_no+1, line))
                raise

        # fix up missing offsets & negative sizes
        last_end = 0x5000 # first offset after partition table
        for e in res:
            if e.offset is None:
                pad_to = 0x10000 if e.type == PartitionDefinition.APP_TYPE else 4
                if last_end % pad_to != 0:
                    last_end += pad_to - (last_end % pad_to)
                e.offset = last_end
            if e.size < 0:
                e.size = -e.size - e.offset
            last_end = e.offset + e.size

        return res

    def __getitem__(self, item):
        """ Allow partition table access via name as well as by
        numeric index. """
        if isinstance(item, str):
            for x in self:
                if x.name == item:
                    return x
            raise ValueError("No partition entry named '%s'" % item)
        else:
            return super(PartitionTable, self).__getitem__(item)

    def verify(self):
        # verify each partition individually
        for p in self:
            p.verify()
        # check for overlaps
        last = None
        for p in sorted(self, key=lambda x:x.offset):
            if p.offset < 0x5000:
                raise InputError("Partition offset 0x%x is below 0x5000" % p.offset)
            if last is not None and p.offset < last.offset + last.size:
                raise InputError("Partition at 0x%x overlaps 0x%x-0x%x" % (p.offset, last.offset, last.offset+last.size-1))
            last = p

class PartitionDefinition(object):
    APP_TYPE = 0x00
    DATA_TYPE = 0x01
    TYPES = {
        "app" : APP_TYPE,
        "data" : DATA_TYPE,
    }

    # Keep this map in sync with esp_partition_subtype_t enum in esp_partition.h 
    SUBTYPES = {
        APP_TYPE : {
            "factory" : 0x00,
            "test" : 0x20,
            },
        DATA_TYPE : {
            "ota" : 0x00,
            "phy" : 0x01,
            "nvs" : 0x02,
            "coredump" : 0x03,
            "esphttpd" : 0x80,
            "fat" : 0x81,
            "spiffs" : 0x82,
            },
    }

    MAGIC_BYTES = b"\xAA\x50"

    ALIGNMENT = {
        APP_TYPE : 0x10000,
        DATA_TYPE : 0x04,
    }

    # dictionary maps flag name (as used in CSV flags list, property name)
    # to bit set in flags words in binary format
    FLAGS = {
        "encrypted" : 0
    }

    # add subtypes for the 16 OTA slot values ("ota_XXX, etc.")
    for ota_slot in range(16):
        SUBTYPES[TYPES["app"]]["ota_%d" % ota_slot] = 0x10 + ota_slot

    def __init__(self):
        self.name = ""
        self.type = None
        self.subtype = None
        self.offset = None
        self.size = None
        self.encrypted = False

    @classmethod
    def from_csv(cls, line):
        """ Parse a line from the CSV """
        line_w_defaults = line + ",,,,"  # lazy way to support default fields
        fields = [ f.strip() for f in line_w_defaults.split(",") ]

        res = PartitionDefinition()
        res.name = fields[0]
        res.type = res.parse_type(fields[1])
        res.subtype = res.parse_subtype(fields[2])
        res.offset = res.parse_address(fields[3])
        res.size = res.parse_address(fields[4])
        if res.size is None:
            raise InputError("Size field can't be empty")

        flags = fields[5].split(":")
        for flag in flags:
            if flag in cls.FLAGS:
                setattr(res, flag, True)
            elif len(flag) > 0:
                raise InputError("CSV flag column contains unknown flag '%s'" % (flag))

        return res

    def __eq__(self, other):
        return self.name == other.name and self.type == other.type \
            and self.subtype == other.subtype and self.offset == other.offset \
            and self.size == other.size

    def __repr__(self):
        def maybe_hex(x):
            return "0x%x" % x if x is not None else "None"
        return "PartitionDefinition('%s', 0x%x, 0x%x, %s, %s)" % (self.name, self.type, self.subtype or 0,
                                                              maybe_hex(self.offset), maybe_hex(self.size))

    def __str__(self):
        return "Part '%s' %d/%d @ 0x%x size 0x%x" % (self.name, self.type, self.subtype, self.offset or -1, self.size or -1)

    def __cmp__(self, other):
        return self.offset - other.offset

    def parse_type(self, strval):
        if strval == "":
            raise InputError("Field 'type' can't be left empty.")
        return parse_int(strval, self.TYPES)

    def parse_subtype(self, strval):
        if strval == "":
            return 0 # default
        return parse_int(strval, self.SUBTYPES.get(self.type, {}))

    def parse_address(self, strval):
        if strval == "":
            return None  # PartitionTable will fill in default
        return parse_int(strval)

    def verify(self):
        if self.type is None:
            raise ValidationError(self, "Type field is not set")
        if self.subtype is None:
            raise ValidationError(self, "Subtype field is not set")
        if self.offset is None:
            raise ValidationError(self, "Offset field is not set")
        align = self.ALIGNMENT.get(self.type, 4)
        if self.offset % align:
            raise ValidationError(self, "Offset 0x%x is not aligned to 0x%x" % (self.offset, align))
        if self.size is None:
            raise ValidationError(self, "Size field is not set")

    STRUCT_FORMAT = "<2sBBLL16sL"


def parse_int(v, keywords={}):
    """Generic parser for integer fields - int(x,0) with provision for
    k/m/K/M suffixes and 'keyword' value lookup.
    """
    try:
        for letter, multiplier in [ ("k",1024), ("m",1024*1024) ]:
            if v.lower().endswith(letter):
                return parse_int(v[:-1], keywords) * multiplier
        return int(v, 0)
    except ValueError:
        if len(keywords) == 0:
            raise InputError("Invalid field value %s" % v)
        try:
            return keywords[v.lower()]
        except KeyError:
            raise InputError("Value '%s' is not valid. Known keywords: %s" % (v, ", ".join(keywords)))

def main():
    global quiet
    parser = argparse.ArgumentParser(description='ESP32 partition table utility')

    parser.add_argument('--verify', '-v', help='Verify partition table fields', default=True, action='store_false')
    parser.add_argument('--quiet', '-q', help="Don't print status messages to stderr", action='store_true')

    args = parser.parse_args()

    quiet = args.quiet
    try:
        os.remove('components/spiffs_image/spiffs_param.mk')
    except:
        pass
    f = open('partitions.csv', 'r')
    input = f.read()
    input = input.decode()
    #status("Parsing CSV input...")
    table = PartitionTable.from_csv(input)

    if args.verify:
        #status("Verifying table...")
        table.verify()

    lastpart = table[len(table)-1]
    if lastpart.name == 'storage':
        #print(lastpart.subtype, "0x%x" % lastpart.offset, lastpart.size)
        f = open('components/spiffs_image/spiffs_param.mk', 'w')
        f.write('PART_SPIFFS_SIZE="%d"\n' % lastpart.size)
        f.write('PART_SPIFFS_BASE_ADDR="0x%x"\n' % lastpart.offset)
        f.close


class InputError(RuntimeError):
    def __init__(self, e):
        super(InputError, self).__init__(e)


class ValidationError(InputError):
    def __init__(self, partition, message):
        super(ValidationError, self).__init__(
            "Partition %s invalid: %s" % (partition.name, message))


if __name__ == '__main__':
    try:
        main()
    except InputError as e:
        print(e, file=sys.stderr)
        sys.exit(2)
