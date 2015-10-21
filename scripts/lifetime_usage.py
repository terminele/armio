#!/bin/python
import logging as log
import struct
import argparse


TICKS_PER_MS = 1

if __name__ == "__main__":
    log.basicConfig(level = log.INFO)
    
    parser = argparse.ArgumentParser(description='Show lifetime usage from a nvm dump')
    parser.add_argument('dumpfile')
    args = parser.parse_args()
    fname = args.dumpfile
    try:
        f = open(fname, 'rb')
    except:
        log.error ("Unable to open file \'{}\'".format(fname))
        exit()

    
    NVM_DATA_SIZE = 12
    binval = f.read(NVM_DATA_SIZE)
    vals = struct.unpack("<BBBBII", binval)
    log.debug("unpack struct: {}".format(struct.unpack("<BBBBII", binval)))
    lifetime_wakes = vals[4]
    lifetime_ticks = vals[5]

    lifetime_s = lifetime_ticks/TICKS_PER_MS/1000.0

    print("Lifetime Wake Count:\t\t {}".format(lifetime_wakes))
    print("Lifetime Active Time (s):\t {:.2f}".format(lifetime_s))
    print("Average Waketime (s):\t\t {:.2f}".format(lifetime_s/lifetime_wakes))

