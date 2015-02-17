
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

    ts = []
    vs = []
    powers = []
    while True:
        binval = f.read(8)

        if not binval or struct.unpack("<IHH", binval)[0] >= 0xffffffff:
            break

        (t,v,p) = struct.unpack("<iHH", binval)

        if len(ts) and t < ts[-1]:
          print("Ignoring out-of-order value t={}.  expect > t: {}".format(t, ts[-1]))
          continue

        if not len(ts): tstart = t

        if (p == 0xbeef):
          pass
        elif (p == 0xdead):
          powers.append(3.3)
          powers.append(0)
        else:
          print("Ended after encountering ({}\t{}\t{:x})".format(t, v, p))
          break

        ts.append((t - tstart) / 3600.0)
        vs.append(v*4.0/4096)
        print("{}\t{}\t{:x}".format(t, v, p))


    plt.plot(ts, vs, 'r.', label='vbatt')
    plt.plot(ts, powers, 'b-', label='on', drawstyle='steps',
        fillstyle='bottom', alpha = 0.3)
    plt.show()

if __name__ == "__main__":
    main()
