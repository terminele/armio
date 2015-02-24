
import math
import struct
import argparse
import matplotlib.pyplot as plt






def wma(data, alpha):
    result = [data[0]]
    for d in data[1:]:
        result.append(d*alpha + result[-1]*(1-alpha))

    return result

def wta(time, data, alpha):
    result = [data[0]]
    for d, tp, tn in zip(data[1:], time, time[1:]):
        dt = tn - tp
        result.append(d*alpha*dt + result[-1]*(1-alpha*dt))

    return result


if __name__ == "__main__":
    import time
    parser = argparse.ArgumentParser(description='Analyze a usage log dump')
    parser.add_argument('dumpfile')
    args = parser.parse_args()
    fname = args.dumpfile
    try:
        f = open(fname, 'r')
    except:
        print ("Unable to open file \'{}\'".format(fname))
        sys.exit()

    skips = 0
    ts = []
    t_rels = []
    vs = []
    powers = []
    deltas = []
    t_wakes = []
    v_wakes = []
    t_offset = 0
    while True:
        binval = f.read(8)

        if not binval:
          break

        if struct.unpack("<IHH", binval)[0] >= 0xffffffff:
            skips+=1

            if skips > 64:
              print("Ending after skipping 64 values")
              break

            continue

        (t,v,p) = struct.unpack("<iHH", binval)

        if t < 10000:
          print("ignoring bad time val {}".format(t))
          continue
        if len(ts) and t + t_offset < ts[-1]:
          if abs(t - ts[0]) < 20:
            t_offset = ts[-1] - t
            print("t[{}] found reset to t={} at t: {}".format(len(ts),time.ctime(t),
            time.ctime(ts[-1])))
          else:
            print("t[{}] found out of order t={} at t: {}".format(len(ts),time.ctime(t),
              time.ctime(ts[-1])))
            t_offset = ts[-1] - t

        t+=t_offset

        if not len(ts):
          tstart = t
          print("tstart is {}".format(time.ctime(t)))

        if (p == 0xbeef):
          powers.append(3.3)
          if len(ts) > 2:
            #print("wakes after {}s".format(t - ts[-1]))
            v_normal = v*4.0/4096
            deltas.append(v_normal - vs[-1])
            t_wakes.append(t - tstart)
            v_wakes.append(v*4.0/4096)
        elif (p == 0xdead):
          powers.append(0)
          #if len(ts) > 2:
          #  print("sleeps after {}s".format(t - ts[-1]))
        else:
          print("skipping bad value at idx {}".format(len(ts)))
          continue

        ts.append(t)
        t_rels.append(t - tstart)
        vs.append(v*4.0/4096)
        #print("{}\t{}\t{:x}".format(t, v, p))


    t_hrs = [t/3600.0 for t in t_rels]
    t_wake_hrs = [t/3600.0 for t in t_wakes]
    plt.plot(t_hrs, vs, 'r.', label='vbatt')
    plt.plot(t_wake_hrs, v_wakes, 'y-', label='vbatt (wake-only)')
    plt.plot(t_hrs, powers, 'b-', label='on', drawstyle='steps-post',
        fillstyle='bottom', alpha = 0.3)
    plt.show()

    plt.plot(t_wakes, deltas, 'b-', label='vbatt (delta)')
    plt.show()

    t_hrs = [t/3600.0 for t in t_rels]
    vs_quarter = wma(v_wakes, 0.25)
    vs_eighth = wma(v_wakes, 0.125)
    vs_sixteenth = wma(v_wakes, 1/16.0)
    vs_64 = wma(v_wakes, 1/64.0)
    vs_128 = wma(v_wakes, 1/128.0)
    plt.plot(t_wake_hrs, vs_quarter, 'r-', label='vbatt (alpha=1/4)')
    plt.plot(t_wake_hrs, vs_eighth, 'b-', label='vbatt (alpha=1/8)')
    plt.plot(t_wake_hrs, vs_64, 'g-', label='vbatt (alpha=1/64)')
    plt.plot(t_wake_hrs, vs_128, 'y-', label='vbatt (alpha=1/128)')
    plt.show()


    vs_quarter = wta(t_hrs,vs, 0.25)
    vs_eighth = wta(t_hrs,vs, 0.125)
    vs_sixteenth = wta(t_hrs,vs, 1/16.0)
    vs_32 = wta(t_hrs,vs, 1/32.0)
    vs_64 = wta(t_hrs,vs, 1/64.0)
    vs_128 = wta(t_hrs,vs, 1/128.0)
    plt.plot(t_hrs, vs_quarter, 'r-', label='vbatt (alpha=1/4)')
    plt.plot(t_hrs, vs_eighth, 'b-', label='vbatt (alpha=1/8)')
    plt.plot(t_hrs, vs_sixteenth, 'g-', label='vbatt (alpha=1/64)')
    plt.plot(t_hrs, vs_32, 'y-', label='vbatt (alpha=1/128)')
    plt.show()

    dur_hrs = t_hrs[-1] - t_hrs[0]
    print("{} looks over {} hours".format(len(v_wakes), dur_hrs))
    print("Look rate: {} per day".format(len(v_wakes)/(dur_hrs/24)))
