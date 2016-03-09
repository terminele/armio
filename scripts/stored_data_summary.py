#!/bin/python
import logging as log
import struct
import argparse


TICKS_PER_MS = 1

if __name__ == "__main__":
    log.basicConfig(level = log.DEBUG)
    
    parser = argparse.ArgumentParser(description='Show lifetime usage from a nvm dump')
    parser.add_argument('dumpfile')
    args = parser.parse_args()
    fname = args.dumpfile
    try:
        f = open(fname, 'rb')
    except:
        log.error ("Unable to open file \'{}\'".format(fname))
        exit()

    
    NVM_DATA_SIZE = 35
    binval = f.read(NVM_DATA_SIZE)
    fmt = "<BBBBIIIIIHBBBBBBHB"
    vals = struct.unpack(fmt, binval)
    log.debug("unpack struct: {}".format(vals))
    rtc_corr = vals[0]
    lifetime_wakes = vals[4]
    filtered_gestures = vals[5]
    filtered_dclicks = vals[6]
    lifetime_ticks = vals[7]
    lifetime_resets = vals[8]
    wdt_resets = vals[9]
    seconds = vals[10]
    minutes = vals[11]
    hours = vals[12]
    day = vals[13]
    month = vals[14]
    year = vals[16]
    pm = vals[17] > 0
    lifetime_s = lifetime_ticks/TICKS_PER_MS/1000.0
    
    if lifetime_wakes > 0:
        print("Lifetime Wake Count:\t\t {}".format(lifetime_wakes))
        print("Filtered Gestures:\t\t {}".format(filtered_gestures))
        print("Filtered DCLICKS:\t\t {}".format(filtered_dclicks))
        print("Lifetime Active Time (s):\t {:.2f}".format(lifetime_s))
        print("Lifetime Resets:\t\t {}".format(lifetime_resets))
        print("Average Waketime (s):\t\t {:.2f}".format(lifetime_s/lifetime_wakes))
        print("Lifetime WDT Resets:\t\t {}".format(wdt_resets))
    else:
        print("No lifetime data")
     
    if year > 1980 and year < 2100:
        print("Stored Date: {}/{}/{}".format(month, day, year))
        print("Stored Time: {}:{:02}:{}s {}".format(hours, minutes, seconds, "pm" if pm else "am"))
    else:
        print("No stored datetime (or we have time traveled)")

    print("RTC Frequency Correction:\t\t {}ppm".format(rtc_corr))

