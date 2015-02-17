
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

    skips = 0
    ts = []
    t_rels = []
    vs = []
    powers = []
    while True:
        binval = f.read(8)

        if not binval:
          break

        if struct.unpack("<IHH", binval)[0] >= 0xffffffff:
            skips+=1

            if skips > 64:
              break

            continue

        (t,v,p) = struct.unpack("<iHH", binval)

        if len(ts) and t < ts[-1]:
          print("Ignoring out-of-order value t={}.  expect > t: {}".format(t, ts[-1]))
          continue

        if not len(ts): tstart = t

        if (p == 0xbeef):
          powers.append(3.3)
          if len(ts) > 2:
            print("wakes after {}s".format(t - ts[-1]))
        elif (p == 0xdead):
          powers.append(0)
          if len(ts) > 2:
            print("sleeps after {}s".format(t - ts[-1]))
        else:
          print("Ended after encountering ({}\t{}\t{:x})".format(t, v, p))
          break

        ts.append(t)
        t_rels.append(t - tstart)
        vs.append(v*4.0/4096)
        print("{}\t{}\t{:x}".format(t, v, p))


    t_hrs = [t/3600.0 for t in t_rels]
    plt.plot(t_hrs, vs, 'r.', label='vbatt')
    plt.plot(t_hrs, powers, 'b-', label='on', drawstyle='steps-post',
        fillstyle='bottom', alpha = 0.3)
    plt.show()

if __name__ == "__main__":
    main()
