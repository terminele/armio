#!/bin/python

import math
import struct
import argparse
import matplotlib.pyplot as plt
import numpy as np





write_csv = False
plot_raw = True
SAMPLE_INT_MS = 10

def analyze_fifo(f):
    binval = f.read(4)

    """ find first start delimiter"""

    started = False
    while binval:
        if struct.unpack("<I", binval)[0] == 0xaaaaaaaa:
            started = True
            print("Found start pattern 0xaaaaaaaa")
            break

        binval = f.read(4)

    if not started:
        print("Could not find start patttern 0xaaaaaaaa")
        return

    xs = []
    ys = []
    zs = []


    samples = []

    binval = f.read(6)
    while binval:
        if struct.unpack("<Ibb", binval)[0] == 0xeeeeeeee:
            """end of fifo data"""
            print("Found 0xeeeeeeee at 0x{:08x}".format(f.tell()))
            samples.append((xs, ys, zs))
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)
            started = False

            xs = []
            ys = []
            zs = []

        if struct.unpack("<Ibb", binval)[0] == 0xdcdcdcdc:
            print("Found DCLICK at 0x{:08x}".format(f.tell()))
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)

        if struct.unpack("<bbbbbb", binval)[2:3] == 0xddba:
            print("Found BAD FIFO at 0x{:08x}".format(f.tell()))
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)


        if started:
            (xh, xl, yh, yl, zh, zl) = struct.unpack("<bbbbbb", binval)
            xs.append(xl)
            ys.append(yl)
            zs.append(zl)

            binval = f.read(6)
            print("0x{:08x}  {}\t{}\t{}".format(f.tell(), xl, yl, zl))

        else:
            """ Look for magic start code """
            if struct.unpack("<Ibb", binval)[0] == 0xaaaaaaaa:
                print("Found start code 0x{:08x}".format(f.tell()))
                """ retain last 2 bytes """
                binval = binval[-2:]
                binval += f.read(4)
                started = True
            else:
                print("Skipping {} searching for start code".format(binval))
                binval = f.read(6)

        if struct.unpack("<Ibb", binval)[0] == 0xffffffff:
            print("No more data after 0x{:08x}".format(f.tell()))
            break


    import uuid
    import csv

    YSUM_N = 15
    ZSUM_N = 15
    YSUM_N_THS = -140
    ZSUM_N_THS = 140
    sigma_zs = []
    sigma_zs_rev = []
    sigma_ys = []
    sigma_ys_rev = []
    sigma_xs = []
    for i, (xs, ys, zs) in enumerate(samples):
        if len(xs) < 1:
            print("skipping invalid data {}".format((xs,ys,zs)))
            continue
        uid =  str(uuid.uuid4())[-6:]
        zsum_n = sum(zs[:-ZSUM_N-1:-1])
        zsum_10 = sum(zs[:10])
        ysum_n = sum(ys[:10])
        xsum_n = sum(xs[:5])
        delta_z_12 = sum(zs[:-13:-1]) - sum(zs[:12])
        print("dz 12: {}".format(delta_z_12))
        if delta_z_12 < 100: #abs(sum(ys[:-11:-1])) > 80 or ysum_n > -255:
            print("plotting {} with xsum_n {}: ysum_n: {}  zsum_n: {} dy_10: {} dz12: {}".format(uid,
                xsum_n, ysum_n, zsum_n, sum(ys[:-11:-1]), delta_z_12))
            fig = plt.figure()
            plt.title(uid)
            ax = fig.add_subplot(111)
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
    print(sigma_zs)
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
        print("{}\t{}\t{}".format(t, x, z))

        binval = f.read(4)

    plt.plot(ts, xs, 'r-', label='x')
    plt.show()
    plt.plot(ts, zs, 'b-', label='z')
    plt.show()




if __name__ == "__main__":
    global write_csv, plot_raw
    parser = argparse.ArgumentParser(description='Analyze an accel log dump')
    parser.add_argument('dumpfile')
    parser.add_argument('-f', '--fifo', action='store_true', default=True)
    parser.add_argument('-s', '--streamed', action='store_true', default=False)
    parser.add_argument('-w', '--write_csv', action='store_true', default=False)
    parser.add_argument('-p', '--plot_raw', action='store_true', default=False)
    args = parser.parse_args()
    fname = args.dumpfile
    write_csv = args.write_csv
    plot_raw = args.plot_raw
    try:
        f = open(fname, 'rb')
    except:
        print ("Unable to open file \'{}\'".format(fname))
        exit()
    if args.fifo:
        analyze_fifo(f)
    else:
        analyze_streamed(f)
