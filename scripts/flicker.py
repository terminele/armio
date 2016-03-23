#!/usr/bin/ipython -i
""" script for combining image files to make clock hr / min hands
"""

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from __future__ import unicode_literals

try:
    from future_builtins import (ascii, filter, hex, map, oct, zip)
except:
    pass

import os
import random
import time
import math


import matplotlib.pyplot as plt

TOTAL_TIME_MS = 2000
DT_MS = 15
INTERVALS = int(TOTAL_TIME_MS / DT_MS)
TI = [ i * DT_MS for i in xrange(INTERVALS) ]

def ifilt( y, A ):
    """
    find y in A(q) y = q^k x where A(q) = 1 + a1 q^-1 + ... + aNa q^-Na
     so: A = [a1, a2, ..., aNa], and len(A) = Na
    """
    A = [ -1 * ai for ai in A ]
    na = len( A )
    for i in xrange( len(y) ):
        for n in range( na ):
            y[i] += A[n] * y[ max( 0, i - n - 1 ) ]
    return(y)

def get_noise( level, filt ):
    noise = [ random.random() * level for i in xrange(INTERVALS) ]
    nm = sum( noise ) / INTERVALS
    noise = [ ni - nm for ni in noise ]
    if filt:
        noise = lpf( noise, filt )
    return noise

def get_signal( ):
    y = [ (i * 1.0 / INTERVALS)**2 - 0.5 for i in xrange( INTERVALS ) ]
    y = [ (i * 1.0 / INTERVALS)**3 - 0.5 for i in xrange( INTERVALS ) ]
    #y = [ (i * 1.0 / INTERVALS)**4 - 0.5 for i in xrange( INTERVALS ) ]
    y = [ 1 - (1 - i * 1.0 / INTERVALS)**2 - 0.5 for i in xrange( INTERVALS ) ]
    y = [ (i * 1.0 / INTERVALS) - 0.5 for i in xrange( INTERVALS ) ]
    return y

def bin_limit( y ):
    mean = sum( y ) / INTERVALS
    mean = 0
    yb = [ 1 if yi - mean > 0 else 0 for yi in y ]
    return yb

def lpf( signal, tao ):
    """ first order lpf """
    A = [ -1.0 * math.exp( -1.0 / tao ) ]
    lpf_sig = ifilt( signal, A )
    return lpf_sig

def normailze( y, ymin=0, ymax=1 ):
    yr = ymax - ymin
    yn = [ ( yi - min(y) ) / ( max(y) - min(y) ) * yr + ymin for yi in y ]
    return yn

def run_test( noise, filt ):
    y = get_signal( )
    n = get_noise( level=noise, filt=filt )
    yb = bin_limit( [ yi + ni for yi, ni in zip( y, n ) ] )
    lines = plt.plot( TI, y, TI, n, TI, [ybi*.9-.45 for ybi in yb] )
    lines[-1].set_linestyle( "steps-post" )
#    plt.ylim( -0.1, 1.1 )
    plt.show()
    return convert(yb)

def transition_deltas( switches ):
    return [ b[0] - a[0] for a,b in zip(switches[:-1], switches[1:]) ]

def convert( y, dt=DT_MS ):
    yc = -1
    switches = [ ]
    for i, yi in enumerate(y):
        if yi != yc:
            yc = yi
            ti = i * dt
            switches.append( ( ti, yc ) )
    if yc != 1:
        switches.append( (len( y ) * dt, 1) )
    return switches

if __name__ == "__main__":
    result = run_test( noise=1.1, filt=2 )
