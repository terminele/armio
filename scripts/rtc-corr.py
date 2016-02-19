#!/bin/python
import logging as log
import struct
import argparse
import matplotlib.pyplot as plt


TICKS_PER_MS = 1

if __name__ == "__main__":
    log.basicConfig(level = log.DEBUG)
    
    parser = argparse.ArgumentParser();
    parser.add_argument('dumpfile')
    args = parser.parse_args()
    fname = args.dumpfile
    try:
        f = open(fname, 'rb')
    except:
        log.error ("Unable to open file \'{}\'".format(fname))
        exit()

    cnts = []    
    diffs = []
    """ skip to start of data"""
    f.seek(0x100)
    while True:
        binval = f.read(4)
        fmt = "<i"
        vals = struct.unpack(fmt, binval)
        if vals[0] == -1:
            break
        
        if vals[0] == 0:
            continue

        #log.debug("unpack struct: {}".format(vals))
        print(vals[0])
        cnts.append(vals[0])
        diff = vals[0] - 10e6
        print(diff)
        diffs.append(diff)
    
    ppm_diffs = [d/10 for d in diffs]
    plt.plot(ppm_diffs, ".")
