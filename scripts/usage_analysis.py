
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
        f = open(fname, 'rb')
    except:
        print ("Unable to open file \'{}\'".format(fname))
        sys.exit()
    
    f.seek(0x100)
    skips = 0
    ts = []
    t_rels = []
    vs = []
    powers = []
    deltas = []
    t_wakes = []
    v_wakes = []
    v_adcs = []
    t_offset = 0
    while True:
        binval = f.read(4)

        if not binval:
          break

        if struct.unpack("<I", binval)[0] >= 0xffffffff or \
           struct.unpack("<I", binval)[0] == 0:
            skips+=1

            if skips > 1024:
              print("Ending after skipping 1024 values")
              break

            continue
        
        #format is a 24-bit unsigned left packed, then an 8-bit vbatt val
        (v,t0,t1,t2) = struct.unpack("<BBBB", binval)
        t = t_offset + t0 + (t1<<8) + (t2<<16)
        print((v, t0, t1, t2)) 
        if len(ts) and t < ts[-1]:
            dt=ts[-1]-t+1
            print("Encountered out-of-order timestamp -- adding offset of {}".format(dt))
            t_offset+=dt
            t+=dt

        print("{} : {}".format(t, v))
        v_normal = 4*(2048 + (v << 2))/4096
        vs.append(v_normal)
        ts.append(t)

    
    t_hrs = [t/3600.0 for t in ts]
    plt.plot(t_hrs, vs, 'b.-', label='on', drawstyle='steps-post')
    plt.show()
#        fillstyle='bottom', alpha = 0.3)
#
#
#    t_hrs = [t/3600.0 for t in t_rels]
#    t_wake_hrs = [t/3600.0 for t in t_wakes]
#    #plt.plot(t_wake_hrs, v_wakes, 'y-', label='vbatt (wake-only)')
#    plt.plot(t_hrs, powers, 'b-', label='on', drawstyle='steps-post',
#        fillstyle='bottom', alpha = 0.3)
#    vbatt_line, = plt.plot(t_hrs, vs, 'r.', label='vbatt')
#    #ax2 = vbatt_line.axes.twiny()
#    #ax2.plot(range(len(t_hrs)), [0 for i in range(len(t_hrs))]) #add top x-axis with indices
#    plt.show()
#    plt.ylim(0.95*min(vs), 1.05*max(vs) )
#    vbatt_line, = plt.plot(range(len(vs)), vs, 'r.', label='vbatt')
#    #vwakes_line, = plt.plot(range(len(v_wakes)), v_wakes, 'g.', label='vbatt-wakes')
#    plt.show()
#    
#    v_adcs, = plt.plot(range(len(v_adcs)), v_adcs, 'b.', label='adc')
#    #vwakes_line, = plt.plot(range(len(v_wakes)), v_wakes, 'g.', label='vbatt-wakes')
#    plt.show()

#    plt.plot(t_wakes, deltas, 'b-', label='vbatt (delta)')
#    plt.show()
#
#    t_hrs = [t/3600.0 for t in t_rels]
#    vs_quarter = wma(v_wakes, 0.25)
#    vs_eighth = wma(v_wakes, 0.125)
#    vs_sixteenth = wma(v_wakes, 1/16.0)
#    vs_64 = wma(v_wakes, 1/64.0)
#    vs_128 = wma(v_wakes, 1/128.0)
#    plt.plot(t_wake_hrs, vs_quarter, 'r-', label='vbatt (alpha=1/4)')
#    plt.plot(t_wake_hrs, vs_eighth, 'b-', label='vbatt (alpha=1/8)')
#    plt.plot(t_wake_hrs, vs_64, 'g-', label='vbatt (alpha=1/64)')
#    plt.plot(t_wake_hrs, vs_128, 'y-', label='vbatt (alpha=1/128)')
#    plt.show()
#
#
#    vs_quarter = wta(t_hrs,vs, 0.25)
#    vs_eighth = wta(t_hrs,vs, 0.125)
#    vs_sixteenth = wta(t_hrs,vs, 1/16.0)
#    vs_32 = wta(t_hrs,vs, 1/32.0)
#    vs_64 = wta(t_hrs,vs, 1/64.0)
#    vs_128 = wta(t_hrs,vs, 1/128.0)
#    plt.plot(t_hrs, vs_quarter, 'r-', label='vbatt (alpha=1/4)')
#    plt.plot(t_hrs, vs_eighth, 'b-', label='vbatt (alpha=1/8)')
#    plt.plot(t_hrs, vs_sixteenth, 'g-', label='vbatt (alpha=1/64)')
#    plt.plot(t_hrs, vs_32, 'y-', label='vbatt (alpha=1/128)')
#    plt.show()
#
#    dur_hrs = t_hrs[-1] - t_hrs[0]
#    print("{} datapts".format(len(t_hrs)))
#    print("{} looks over {} hours".format(len(v_wakes), dur_hrs))
#    print("Look rate: {} per day".format(len(v_wakes)/(dur_hrs/24)))
