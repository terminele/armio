
import math
import struct
import argparse
import matplotlib.pyplot as plt






SAMPLE_INT_MS = 10

def analyze_fifo(f):
    binval = f.read(4)

    """ find first start delimiter"""

    started = False
    while binval:
        if struct.unpack("<I", binval)[0] == 0xaaaaaaaa:
            started = True
            break

        binval = f.read(4)

    if not started:
        print("Could not find start pattter 0xaaaaaaaa")
        return

    xs = []
    ys = []
    zs = []


    samples = []

    binval = f.read(6)
    while binval:
        if struct.unpack("<Ibb", binval)[0] == 0xeeeeeeee:
            """end of fifo data"""
            print("Found 0xeeeeeeee")
            samples.append((xs, ys, zs))
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)
            started = False

            xs = []
            ys = []
            zs = []

        if struct.unpack("<Ibb", binval)[0] == 0xdcdcdcdc:
            print("Found DCLICK")
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)

        if struct.unpack("<bbbbbb", binval)[2:3] == 0xddba:
            print("Found BAD FIFO")
            """ retain last 2 bytes """
            binval = binval[-2:]
            binval += f.read(4)

        if started:
            (xh, xl, yh, yl, zh, zl) = struct.unpack("<bbbbbb", binval)
            xs.append(xl)
            ys.append(yl)
            zs.append(zl)

            print("{}\t{}\t{}".format(xl, yl, zl))

        else:
            """ Look for magic start code """
            if struct.unpack("<Ibb", binval)[0] == 0xaaaaaaaa:
                print("Found new start")
                """ retain last 2 bytes """
                binval = binval[-2:]
                binval += f.read(4)
                started = True

        if struct.unpack("<Ibb", binval)[0] == 0xffffffff:
            print ("No more data")
            break

        binval = f.read(6)

    for (xs, ys, zs) in samples:
        print("plotting {}".format(xs))
        fig = plt.figure()
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
    parser = argparse.ArgumentParser(description='Analyze an accel log dump')
    parser.add_argument('dumpfile')
    parser.add_argument('-f', '--fifo', action='store_true', default=True)
    parser.add_argument('-s', '--streamed', action='store_true', default=False)
    args = parser.parse_args()
    fname = args.dumpfile
    try:
        f = open(fname, 'r')
    except:
        print ("Unable to open file \'{}\'".format(fname))
        exit()
    if args.fifo:
        analyze_fifo(f)
    else:
        analyze_streamed(f)
