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

    
    NVM_DATA_SIZE = 27
    binval = f.read(NVM_DATA_SIZE)
    fmt = "<BBBBIIIHBBBBBBHB"
    vals = struct.unpack(fmt, binval)
    log.debug("unpack struct: {}".format(vals))
    lifetime_wakes = vals[4]
    lifetime_ticks = vals[5]
    lifetime_resets = vals[6]
    wdt_resets = vals[7]
    seconds = vals[8]
    minutes = vals[9]
    hours = vals[10]
    day = vals[11]
    month = vals[12]
    year = vals[14]
    pm = vals[15] > 0
    lifetime_s = lifetime_ticks/TICKS_PER_MS/1000.0

    print("Lifetime Wake Count:\t\t {}".format(lifetime_wakes))
    print("Lifetime Active Time (s):\t {:.2f}".format(lifetime_s))
    print("Lifetime Resets:\t\t {}".format(lifetime_resets))
    print("Average Waketime (s):\t\t {:.2f}".format(lifetime_s/lifetime_wakes))
    
    
    print("Lifetime WDT Resets:\t\t {}".format(lifetime_resets))
    print("Stored Date: {}/{}/{}".format(month, day, year))
    print("Stored Time: {}:{:02}:{}s {}".format(hours, minutes, seconds, "pm" if pm else "am"))

