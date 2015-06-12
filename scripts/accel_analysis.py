
import math
import struct
import argparse
import matplotlib.pyplot as plt









def main():
    parser = argparse.ArgumentParser(description='Analyze an accel log dump')
    parser.add_argument('dumpfile')
    args = parser.parse_args()
    fname = args.dumpfile
    try:
        f = open(fname, 'r')
    except:
        print ("Unable to open file \'{}\'".format(fname))
        return

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
    main()
