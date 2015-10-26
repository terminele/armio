#!/bin/python

import math
import struct
import argparse
import matplotlib.pyplot as plt
import numpy as np
import logging as log
import uuid
import csv


TICKS_PER_MS = 1

write_csv = False
SAMPLE_INT_MS = 10
rejecttime = 0
args = None

class WakeSample:
    _sample_counter = 0
    def __init__(self, xs, ys, zs, waketime = 0, confirmed = False):
        self.xs = xs
        self.ys = ys
        self.zs = zs
        self.waketime = waketime
        self.confirmed = confirmed
        WakeSample._sample_counter+=1
        self.i = WakeSample._sample_counter

    def wakeup_check(self):
        wakeup = False
        XSUM_N = 5
        YSUM_N = 8
        ZSUM_N = 11
        YSUM_THS = 220
        XSUM_THS = 120
        DZ_THS = 110
        Y_LATEST_THS = 6
        zsum_last_n = sum(self.zs[:-ZSUM_N-1:-1])
        zsum_first_n = sum(self.zs[:ZSUM_N])
        ysum_n = sum(self.ys[:YSUM_N])
        xsum_n = sum(self.xs[:XSUM_N])
        dzN = zsum_last_n - zsum_first_n
        
        if dzN > DZ_THS:
            log.info("{}: passes dzN".format(self.i))
            wakeup = True

        if abs(ysum_n) >= YSUM_THS:
            log.info("{}: passes YSUM_N THS".format(self.i))
            wakeup = True
        
        if abs(xsum_n) >= XSUM_THS:
            log.info("{}: passes XSUM_N THS".format(self.i))
            wakeup = True
        
        if self.ys[-1] >= Y_LATEST_THS:
            log.info("{}: passes Y_LAST THS".format(self.i))
            wakeup = True
        
        if not wakeup and not self.confirmed:
            log.info("{}: correctly rejected with dzN={}  XSUM_N={}  YSUM_N={}  Y_LAST={}".format(
                self.i, dzN, xsum_n, ysum_n, self.ys[-1]))
        elif not wakeup and self.confirmed:
            log.error("{}: FALSELY rejected with dzN={}  XSUM_N={}  YSUM_N={}  Y_LAST={}".format(
                self.i, dzN, xsum_n, ysum_n, self.ys[-1]))
        

        return wakeup

def parse_fifo(fname):
    try:
        f = open(fname, 'rb')
    except:
        log.error ("Unable to open file \'{}\'".format(fname))
        return []
    
    binval = f.read(4)

    """ find first start delimiter"""

    started = False
    while binval:
        if struct.unpack("<I", binval)[0] == 0xaaaaaaaa:
            started = True
            log.debug("Found start pattern 0xaaaaaaaa")
            break

        binval = f.read(4)

    if not started:
        log.error("Could not find start patttern 0xaaaaaaaa")
        return

    xs = []
    ys = []
    zs = []


    samples = []
    
    """ loop over binary file in chunks of 6 bytes
        and parse out:
            FIFO Start Code - 0xaaaaaaaa
            FIFO Data - 32x3x2-bytes
            FIFO End Code - 0xeeeeeeeee
            (optional) waketime log code 0xddee followed by waketime ticks as unsigned int 
    """
    i = 0
    binval = f.read(6)
    while binval:
        if len(binval) != 6:
            log.info("end of file encountered")
            break 
        data = struct.unpack("<IBB", binval)
        end_unconfirm = (data[0] == 0xeeeeeeee)#FIFO end code
        end_confirm = (data[0] == 0xcccccccc) #FIFO end code
        if end_confirm or end_unconfirm:
            """end of fifo data"""
            log.debug("Found fifo end at 0x{:08x}".format(f.tell()))
            
            if data[1:] == (0xdd, 0xee): #waketime log code
                binval = f.read(4)
                waketime_ticks = struct.unpack("<I", binval)[0]
                waketime_ms = waketime_ticks/TICKS_PER_MS 
                binval = f.read(6) # update binval with next 6 bytes
            else:
                """ waketime not include in this log dump"""
                waketime_ms = 0
                binval = binval[-2:] #retain last 2 bytes
                binval += f.read(4)
            
            samples.append(WakeSample(xs,ys,zs, waketime_ms, end_confirm))
            i+=1
            log.debug("new sample: waketime={}ms confirmed={}".format(waketime_ms,
                end_confirm))
            started = False

            xs = []
            ys = []
            zs = []

        if struct.unpack("<Ibb", binval)[0] == 0xdcdcdcdc:
            log.debug("Found DCLICK at 0x{:08x}".format(f.tell()))
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)

        if struct.unpack("<bbbbbb", binval)[2:3] == (0xdd, 0xba):
            log.debug("Found BAD FIFO at 0x{:08x}".format(f.tell()))
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)


        if started:
            (xh, xl, yh, yl, zh, zl) = struct.unpack("<bbbbbb", binval)
            xs.append(xl)
            ys.append(yl)
            zs.append(zl)

            binval = f.read(6)
            log.debug("0x{:08x}  {}\t{}\t{}".format(f.tell(), xl, yl, zl))

        else:
            """ Look for magic start code """
            if len(binval) != 6:
                log.info("end of file encountered")
                break

            if struct.unpack("<Ibb", binval)[0] == 0xaaaaaaaa:
                log.debug("Found start code 0x{:08x}".format(f.tell()))
                """ retain last 2 bytes """
                binval = binval[-2:]
                binval += f.read(4)
                started = True
            else:
                log.debug("Skipping {} searching for start code".format(binval))
                binval = f.read(6)

        if len(binval) != 6:
            log.info("end of file encountered")
            break
        if struct.unpack("<Ibb", binval)[0] == 0xffffffff:
            log.debug("No more data after 0x{:08x}".format(f.tell()))
            break
    
    log.info("Parsed {} samples from {}".format(len(samples), fname))

    return samples

def analyze_samples(samples):
    log.info("Analyzing {} FIFO Samples".format(len(samples)))
    
    wakeups = 0
    rejects = 0
    false_negatives = 0
    confirmed_wakeup_passes = 0
    
    sigma_zs = []
    sigma_zs_rev = []
    sigma_ys = []
    sigma_ys_rev = []
    sigma_xs = []
    for (i, sample) in enumerate(samples):
        xs = sample.xs
        ys = sample.ys
        zs = sample.zs
        waketime  = sample.waketime
        confirmed = sample.confirmed

        if len(xs) < 1:
            log.debug("skipping invalid data {}".format((xs,ys,zs)))
            continue
        uid =  str(uuid.uuid4())[-6:]
        ZSUM_N = 11
        zsum_n = sum(zs[:-ZSUM_N-1:-1])
        zsum_10 = sum(zs[:10])
        ysum_n = sum(ys[:10])
        xsum_n = sum(xs[:5])
        delta_z_12 = sum(zs[:-13:-1]) - sum(zs[:12])
        wakeup_check_pass = sample.wakeup_check()
        if wakeup_check_pass: 
            wakeups+=1
            if confirmed: confirmed_wakeup_passes +=1
        else:
            rejects+=1
            global rejecttime
            rejecttime+=waketime
            if confirmed: false_negatives+=1
        
        
        if args.plot_all or (wakeup_check_pass and args.plot_passes) or \
                (not wakeup_check_pass and args.plot_rejects):
            log.debug("plotting {} with xsum_n {}: ysum_n: {}  zsum_n: {} dy_10: {} dz12: {}".format(uid,
                xsum_n, ysum_n, zsum_n, sum(ys[:-11:-1]), delta_z_12))
            fig = plt.figure()
            ax = fig.add_subplot(111)
            plt.title("sample {}: pass={}  confirmed={}".format(i, 
                wakeup_check_pass, confirmed))
            ax.grid()
            t = range(len(xs))
            x_line = plt.Line2D(t, xs, color='r', marker='o')
            y_line = plt.Line2D(t, ys, color = 'g', marker='o')
            z_line = plt.Line2D(t, zs, color = 'b', marker='o')

            ax.add_line(x_line)
            ax.add_line(y_line)
            ax.add_line(z_line)

            plt.figlegend((x_line, y_line, z_line), ("x", "y", "z"), "upper right")
            ax.set_xlim([0, len(xs)])
            ax.set_ylim([-60, 60])

            plt.show()

        sigma_zs.append(np.cumsum(zs))
        sigma_zs_rev.append(np.cumsum(zs[::-1]))
        sigma_ys.append(np.cumsum(ys))
        sigma_ys_rev.append(np.cumsum(ys[::-1]))
        sigma_xs.append(np.cumsum(xs))

        if write_csv:
            with open('{}.csv'.format(uid), 'wt') as csvfile:
                writer = csv.writer(csvfile, delimiter=' ')
                for i, (x, y, z) in enumerate(zip(xs, ys, zs)):
                    writer.writerow([i, x, y, z])
    
    log.info("{} wakeups ({} %)".format(wakeups, 100.0*wakeups/len(samples)))
    log.info("{} rejects ({} %)".format(rejects, 100.0*rejects/len(samples)))
    total_waketime = sum([s.waketime for s in samples])
    log.info("{:.2f}s total unfiltered waketime".format(total_waketime/1000.0))
    log.info("{:.2f}s total reject-saved waketime ({:.1f}%)".format(
        rejecttime/1000.0, 100*rejecttime/total_waketime))
    
    total_confirms = confirmed_wakeup_passes + false_negatives
    log.info("{} confirmed wakeup passes ({} %)".format(confirmed_wakeup_passes, 
        100*confirmed_wakeup_passes/total_confirms))
    log.info("{} false_negatives ({} %)".format(false_negatives, 
        100*false_negatives/total_confirms))
     
    if args.plot_csums:
        fig = plt.figure()
        plt.title("Sigma x-values")
        for vals in sigma_xs:
            t = range(len(vals))
            ax = fig.add_subplot(111)
            ax.grid()
            line = plt.Line2D(t, vals, marker='o')#, color='r', marker='o')
            ax.add_line(line)

        ax.relim()
        ax.autoscale_view()
        plt.show()
        fig = plt.figure()
        plt.title("Sigma y-values")
        for cys in sigma_ys:
            t = range(len(cys))
            ax = fig.add_subplot(111)
            ax.grid()
            line = plt.Line2D(t, cys, marker='o')#, color='r', marker='o')
            ax.add_line(line)

        ax.relim()
        ax.autoscale_view()
        plt.show()

        fig = plt.figure()
        plt.title("Sigma y-values reverse")
        for vals in sigma_ys_rev:
            t = range(len(vals))
            ax = fig.add_subplot(111)
            ax.grid()
            line = plt.Line2D(t, vals, marker='o')#, color='r', marker='o')
            ax.add_line(line)

        ax.relim()
        ax.autoscale_view()
        plt.show()

        fig = plt.figure()
        plt.title("Sigma z-values")
        for vals in sigma_zs:
            t = range(len(vals))
            ax = fig.add_subplot(111)
            ax.grid()
            line = plt.Line2D(t, vals, marker='o')#, color='r', marker='o')
            ax.add_line(line)

        ax.relim()
        ax.autoscale_view()
        plt.show()

        fig = plt.figure()
        plt.title("Sigma z-values reverse")
        for vals in sigma_zs_rev:
            t = range(len(vals))
            ax = fig.add_subplot(111)
            ax.grid()
            line = plt.Line2D(t, vals, marker='o')#, color='r', marker='o')
            ax.add_line(line)

        ax.relim()
        ax.autoscale_view()
        plt.show()

        plt.title("dz delta N")
        ns = range(8,20)
        xs = []
        dzs = []
        log.debug(sigma_zs)
        for n in ns:
            dzs.extend([(czs[-1] - czs[-n-1]) - czs[n-1] for czs in sigma_zs])
            xs.extend([n for i in range(len(sigma_zs))])

        plt.scatter(xs, dzs)
        plt.show()

def analyze_streamed(f):
    binval = f.read(4)
    #skip any leading 0xffffff bytes
    while struct.unpack("<I", binval)[0] == 0xffffffff:
        binval = f.read(4)

    ts = []
    xs = []
    zs = []
    while binval:
        if struct.unpack("<I", binval)[0] == 0xffffffff:
            break

        (z,x,t) = struct.unpack("<bbH", binval)
        ts.append(t)
        xs.append(x)
        zs.append(z)
        log.debug("{}\t{}\t{}".format(t, x, z))

        binval = f.read(4)

    plt.plot(ts, xs, 'r-', label='x')
    plt.show()
    plt.plot(ts, zs, 'b-', label='z')
    plt.show()




if __name__ == "__main__":
    global write_csv, args
    log.basicConfig(level = log.INFO)
    parser = argparse.ArgumentParser(description='Analyze an accel log dump')
    parser.add_argument('dumpfiles', nargs='+')
    parser.add_argument('-f', '--fifo', action='store_true', default=True)
    parser.add_argument('-s', '--streamed', action='store_true', default=False)
    parser.add_argument('-w', '--write_csv', action='store_true', default=False)
    parser.add_argument('-a', '--plot_all', action='store_true', default=False)
    parser.add_argument('-r', '--plot_rejects', action='store_true', default=False)
    parser.add_argument('-p', '--plot_passes', action='store_true', default=False)
    parser.add_argument('-c', '--plot_csums', action='store_true', default=False)
    
    args = parser.parse_args()
    write_csv = args.write_csv
    
    if args.fifo:
        samples = []
        for fname in args.dumpfiles:
            samples.extend(parse_fifo(fname))

        analyze_samples(samples)
    else:
        fname = args.dumpfiles[0]
        try:
            f = open(fname, 'rb')
        except:
            log.error ("Unable to open file \'{}\'".format(fname))
            exit()
        analyze_streamed(f)
