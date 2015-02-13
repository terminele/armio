
import math
import struct
import argparse
import matplotlib.pyplot as plt









def main():
    parser = argparse.ArgumentParser(description='Analyze a usage log dump')
    parser.add_argument('dumpfile')
    args = parser.parse_args()
    fname = args.dumpfile
    try:
        f = open(fname, 'r')
    except:
        print ("Unable to open file \'{}\'".format(fname))
        return

    binval = f.read(8)
    ts = []
    vs = []
    powers = []
    while binval:
        if struct.unpack("<IHH", binval)[0] >= 0xffffffff:
            break

        (t,v,p) = struct.unpack("<iHH", binval)


        if (p == 0xbeef):
          powers.append(1)
        elif (p == 0xdead):
          powers.append(0)
        else:
          print("Ended after encountering ({}\t{}\t{:x})".format(t, v, p))
          break

        ts.append(t)
        vs.append(v*4.0/4096)
        print("{}\t{}\t{:x}".format(t, v, p))

        binval = f.read(8)

    plt.plot(ts, vs, 'r-', label='x')
    plt.plot(ts, powers, 'b-', label='z')
    plt.show()

if __name__ == "__main__":
    main()
