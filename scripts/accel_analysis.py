#!/bin/python

import math
import struct
import argparse
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
import logging as log
import uuid
import csv
import random

TICKS_PER_MS = 1

SAMPLE_INT_MS = 10
rejecttime = 0
args = None

REJECT, PASS, ACCEPT = range( 3 )


class SampleTest( object ):
    def __init__( self, name, test_fcn, **kwargs ):
        self.name = name
        self._test_fcn = test_fcn
        self._reject_below = kwargs.pop( "reject_below", None )
        self._reject_above = kwargs.pop( "reject_above", None )
        self._accept_below = kwargs.pop( "accept_below", None )
        self._accept_above = kwargs.pop( "accept_above", None )
        self.samples = []
        self.values = []
        self.clear_results()

    def clear_samples( self ):
        self.samples = []
        self.values = []
        self.clear_results()

    def _getPassedSamples( self ):
        if not self.analyzed:
            self.analyze()
        return self._passed_samples
    passed_samples = property( fget=_getPassedSamples )

    def clear_results( self ):
        self.analyzed = False
        self.total = 0

        self.test_results = []

        self.rejected = 0
        self.passed = 0
        self._passed_samples = []
        self.accepted = 0

        self.confirmed = 0
        self.unconfirmed = 0

        self.confirmed_accepted = 0
        self.confirmed_passed = 0
        self.confirmed_rejected = 0

        self.unconfirmed_accepted = 0
        self.unconfirmed_passed = 0
        self.unconfirmed_rejected = 0

        self.unconfirmed_time = 0
        self.unconfirmed_time_cnt = 0
        self.unconfirmed_time_max = 0

        self.unconfirmed_rejected_time = 0
        self.unconfirmed_passed_time = 0
        self.unconfirmed_accepted_time = 0

    def _test_sample( self, value, fltr_num=None ):
        """ if value is multidimensional, filters
            must also be multi dimensional (this is for 'and' style) """
        if isinstance( value, ( tuple, list ) ):
            tests = [ self._test_sample( vi, i ) for i, vi in enumerate( value ) ]
            if all( test == ACCEPT for test in tests ):
                return ACCEPT
            elif all( test == REJECT for test in tests ):
                return REJECT
            else:
                return PASS

        if fltr_num is None:
            rb = self._reject_below
            ra = self._reject_above
            ab = self._accept_below
            aa = self._accept_above
        else:
            if self._reject_below is None:
                rb = None
            else:
                rb = self._reject_below[ fltr_num ]
            if self._reject_above is None:
                ra = None
            else:
                ra = self._reject_above[ fltr_num ]
            if self._accept_below is None:
                ab = None
            else:
                ab = self._accept_below[ fltr_num ]
            if self._accept_above is None:
                aa = None
            else:
                aa = self._accept_above[ fltr_num ]

        if rb is not None:
            if value < rb:
                return REJECT
        if ra is not None:
            if value > ra:
                return REJECT
        if aa is not None:
            if value > aa:
                return ACCEPT
        if ab is not None:
            if value < ab:
                return ACCEPT
        return PASS

    def add_samples( self, samples ):
        for sample in samples:
            if len( sample.xs ) == 0:
                continue
            self.analyzed = False
            val = self._test_fcn( sample )
            self.samples.append( sample )
            self.values.append( val )

    def analyze( self ):
        self.clear_results()
        for val, sample in zip( self.values, self.samples ):
            self.total += 1
            test = self._test_sample( val )
            self.test_results.append( test )
            if test == ACCEPT:
                self.accepted += 1
            elif test == REJECT:
                self.rejected += 1
            elif test == PASS:
                self.passed += 1
                self._passed_samples.append( sample )

            if sample.confirmed:
                self.confirmed += 1
                if test == ACCEPT:
                    self.confirmed_accepted += 1
                elif test == REJECT:
                    self.confirmed_rejected += 1
                elif test == PASS:
                    self.confirmed_passed += 1
            else:
                self.unconfirmed += 1
                if test == ACCEPT:
                    self.unconfirmed_accepted += 1
                elif test == REJECT:
                    self.unconfirmed_rejected += 1
                elif test == PASS:
                    self.unconfirmed_passed += 1

                if sample.waketime:
                    self.unconfirmed_time_cnt += 1
                    self.unconfirmed_time += sample.waketime
                    if sample.waketime > self.unconfirmed_time_max:
                        self.unconfirmed_time_max = sample.waketime
                    if test == ACCEPT:
                        self.unconfirmed_accepted_time += sample.waketime
                    elif test == REJECT:
                        self.unconfirmed_rejected_time += sample.waketime
                    elif test == PASS:
                        self.unconfirmed_passed_time += sample.waketime
        self.analyzed = True

    def show_result( self ):
        if not self.analyzed:
            self.analyze()
        print( "Analysis for '{}'".format( self.name ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "", "Confirmed", "Unconfirmed", "Totals" ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "Accepted", self.confirmed_accepted, self.unconfirmed_accepted, self.accepted ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "Passed", self.confirmed_passed, self.unconfirmed_passed, self.passed ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "Rejected", self.confirmed_rejected, self.unconfirmed_rejected, self.rejected ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "Total", self.confirmed, self.unconfirmed, self.total ) )
        if self.unconfirmed_time_cnt:
            print( "Unconfirmed time analysis ({} values):".format( self.unconfirmed_time_cnt ) )
            print( "  average unconfirmed time {:.1f} seconds".format(
                1e-3 * self.unconfirmed_time / self.unconfirmed_time_cnt ) )
            print( "  maximum unconfirmed time {:.1f} seconds".format(
                1e-3 * self.unconfirmed_time_max ) )
            print( "  {:.1f} of {:.1f} unconf sec accepted, {:.0%}".format(
                self.unconfirmed_accepted_time * 1e-3, self.unconfirmed_time * 1e-3,
                self.unconfirmed_accepted_time / self.unconfirmed_time ) )
            print( "  {:.1f} of {:.1f} unconf sec passed, {:.0%}".format(
                self.unconfirmed_passed_time * 1e-3, self.unconfirmed_time * 1e-3,
                self.unconfirmed_passed_time / self.unconfirmed_time ) )
            print( "  {:.1f} of {:.1f} unconf sec rejected, {:.0%}".format(
                self.unconfirmed_rejected_time * 1e-3, self.unconfirmed_time * 1e-3,
                self.unconfirmed_rejected_time / self.unconfirmed_time ) )
        print()

    def plot_result( self ):
        if not self.analyzed:
            self.analyze()
        ths = ( self._reject_below, self._reject_above,
                self._accept_below, self._accept_above )
        dims = set( len( th ) if isinstance( th, ( list, tuple ) ) else 1
                for th in ths if th is not None )
        if len( dims ) != 1:
            raise ValueError( "{} has {} dimensions".format( self.name, dims ) )
        dim = dims.pop()
        if dim > 3 or dim == 0:
            raise ValueError( "Don't know how to plot {} dimensions".format( dim ) )

        wake_short = self.unconfirmed_time_max / 4
        wake_med = self.unconfirmed_time_max / 2

        CONFIRMED, SHORT, MED, LONG = range( 4 )

        lines = dict()
        for tv in ( PASS, ACCEPT, REJECT ):
            for cv in ( CONFIRMED, SHORT, MED, LONG ):
                lines[ (tv, cv) ] = []

        if (len( self.test_results ) != len( self.values ) or
                len( self.samples ) != len( self.test_results )):
            raise ValueError( "We are missing something {} {} {}".format(
                len( self.test_results ), len( self.values ), len( self.samples ) ))
        log.info( "Plotting {} samples".format( len( self.values ) ) )

        for tv, val, sample in zip( self.test_results, self.values, self.samples ):
            if sample.confirmed:
                cv = CONFIRMED
            elif sample.waketime == 0:
                cv = MED
            elif sample.waketime < wake_short:
                cv = SHORT
            elif sample.waketime < wake_med:
                cv = MED
            else:
                cv = LONG
            if dim == 1:
                val = (sample.i, val)
            lines[ ( tv, cv ) ].append( val )

        fig = plt.figure()
        if dim == 3:
            #kw = {'projection': '3d'} if dim == 3 else dict()
            #ax_sigx = fig.add_subplot( 111, *kw )
            ax_sigx = Axes3D( fig )    # old way of doing this
        else:
            ax_sigx = fig.add_subplot( 111 )
        ax_sigx.set_title( self.name )
        markers = [ 'x', '.', 'o' ]
        colors = [ 'green', 'yellow', 'orange', 'red' ]

        for ( test, confirm ), data in lines.items():
            if not len( data ):
                continue
            marker = markers[test]
            color = colors[confirm]
            if dim == 3:
                xs, ys, zs = list( zip( *data ) )
                plt.scatter( xs, ys, zs, c=color, marker=marker )
                #plt.scatter( xs, ys, zs=zs, c=color )
            else:
                xs, ys = list( zip( *data ) )
                plt.scatter( xs, ys, color=color, marker=marker )

        ths = ( self._reject_below, self._reject_above,
                self._accept_below, self._accept_above )

        for th in ths:
            if th is None:
                continue
            elif dim == 1:
                xs = plt.xlim()
                ys = [ th, th ]
                plt.plot( plt.xlim(), [th, th], 'k-' )
            elif dim == 2:
                plt.plot( [th[0], th[0]], plt.ylim(), 'k-' )
                plt.plot( plt.xlim(), [th[1], th[1]], 'k-' )
            elif dim == 3:
                pass        # for now
        ax_sigx.autoscale( tight=True )

        plt.show()


class Samples( object ):
    def __init__( self, name=None ):
        self.samples = []
        self.name = name
        self.clear_results()

    def clear_results( self ):
        self.total = 0
        self.rejected = 0
        self.accepted = 0

        self.confirmed = 0
        self.unconfirmed = 0

        self.confirmed_accepted = 0
        self.confirmed_rejected = 0
        self.unconfirmed_accepted = 0
        self.unconfirmed_rejected = 0

        self.unconfirmed_time = 0
        self.unconfirmed_rejected_time = 0
        self.unconfirmed_time_cnt = 0
        self.unconfirmed_time_max = 0

    def analyze( self ):
        self.clear_results()
        for sample in self.samples:
            if len( sample.xs ) < 1:
                log.debug( "skipping invalid data {}".format(
                    (sample.xs, sample.ys, sample.zs) ) )
                continue

            self.total += 1
            if sample.wakeup_check( force=True ):
                self.accepted += 1
            else:
                self.rejected += 1
            if sample.confirmed:
                self.confirmed += 1
                if sample.accepted:
                    self.confirmed_accepted += 1
                else:
                    self.confirmed_rejected += 1
            else:
                self.unconfirmed += 1
                if sample.accepted:
                    self.unconfirmed_accepted += 1
                else:
                    self.unconfirmed_rejected += 1
                if sample.waketime:
                    self.unconfirmed_time_cnt += 1
                    self.unconfirmed_time += sample.waketime
                    if not sample.accepted:
                        self.unconfirmed_rejected_time += sample.waketime
                    if sample.waketime > self.unconfirmed_time_max:
                        self.unconfirmed_time_max = sample.waketime

    def show_analysis( self ):
        print( "Analysis for '{}'".format( self.name ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "", "Confirmed", "Unconfirmed", "Totals" ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "Accepted", self.confirmed_accepted, self.unconfirmed_accepted, self.accepted ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "Rejected", self.confirmed_rejected, self.unconfirmed_rejected, self.rejected ) )
        print( "{:10}|{:12}|{:12}|{:12}".format( "Total", self.confirmed, self.unconfirmed, self.total ) )
        if self.unconfirmed_time_cnt:
            print( "Unconfirmed time analysis ({} values):".format( self.unconfirmed_time_cnt ) )
            print( "  average unconfirmed time {:.1f} seconds".format(
                1e-3 * self.unconfirmed_time / self.unconfirmed_time_cnt ) )
            print( "  maximum unconfirmed time {:.1f} seconds".format(
                1e-3 * self.unconfirmed_time_max ) )
            print( "  {:.1f} of {:.1f} seconds filtered, {:.0%}".format(
                self.unconfirmed_rejected_time * 1e-3, self.unconfirmed_time * 1e-3,
                self.unconfirmed_rejected_time / self.unconfirmed_time ) )
        print()

    def load( self, fn ):
        if self.name is None:
            self.name = fn
        newsamples = self._parse_fifo( fn )
        self.samples.extend( newsamples )

    def combine( self, other ):
        if self.name is None:
            self.name = "Combined"
        self.samples.extend( other.samples )
        self.total += other.total
        self.rejected += other.rejected
        self.accepted += other.accepted

        self.confirmed += other.confirmed
        self.unconfirmed += other.unconfirmed

        self.confirmed_accepted += other.confirmed_accepted
        self.confirmed_rejected += other.confirmed_rejected
        self.unconfirmed_accepted += other.unconfirmed_accepted
        self.unconfirmed_rejected += other.unconfirmed_rejected

        self.unconfirmed_time += other.unconfirmed_time
        self.unconfirmed_rejected_time += other.unconfirmed_rejected_time
        self.unconfirmed_time_cnt += other.unconfirmed_time_cnt

        if other.unconfirmed_time_max > self.unconfirmed_time_max:
            self.unconfirmed_time_max = other.unconfirmed_time_max

    def filter_samples( self, **kwargs ):
        reject = kwargs.pop( "reject_only", False )
        accept = kwargs.pop( "accept_only", False )
        fnonly = kwargs.pop( "false_negatives", False )
        for sample in self.samples:
            if reject and sample.accepted:
                continue
            elif accept and not sample.accepted:
                continue
            elif fnonly and ( not sample.confirmed or sample.accepted ):
                continue
            yield sample

    def show_plots( self, **kwargs ):
        for sample in self.filter_samples( **kwargs ):
            sample.show_plot()

    def export_csv( self, **kwargs ):
        for sample in self.filter_samples( **kwargs ):
            sample.export_csv()

    def show_csums( self, **kwargs ):
        fig = plt.figure()
        ax_sigx = fig.add_subplot(111)
        ax_sigx.set_title( "Sigma x-values" )

        fig = plt.figure()
        ax_sigx_rev = fig.add_subplot(111)
        ax_sigx_rev.set_title( "Sigma x-values reverse" )

        fig = plt.figure()
        ax_sigy = fig.add_subplot(111)
        ax_sigy.set_title( "Sigma y-values" )

        fig = plt.figure()
        ax_sigy_rev = fig.add_subplot(111)
        ax_sigy_rev.set_title( "Sigma y-values reverse" )

        fig = plt.figure()
        ax_sigz = fig.add_subplot(111)
        ax_sigz.set_title( "Sigma z-values" )

        fig = plt.figure()
        ax_sigz_rev = fig.add_subplot(111)
        ax_sigz_rev.set_title( "Sigma z-values reverse" )

        vals = 0
        xs_confirm = []
        dzs_confirm = []
        xs_unconfirm = []
        dzs_unconfirm = []
        for sample in self.filter_samples( **kwargs ):
            vals += 1
            t = range( len( sample.xsums ) )
            c = 'b' if sample.confirmed else 'r'


            line = plt.Line2D( t, sample.xsums, marker='o' , color=c )
            ax_sigx.add_line( line )

            line = plt.Line2D( t, sample.ysums, marker='o', color=c )
            ax_sigy.add_line( line )

            line = plt.Line2D( t, sample.zsums, marker='o', color=c )
            ax_sigz.add_line( line )

            line = plt.Line2D( t, sample.xsumrev, marker='o', color=c )
            ax_sigx_rev.add_line( line )

            line = plt.Line2D( t, sample.ysumrev, marker='o', color=c )
            ax_sigy_rev.add_line( line )

            line = plt.Line2D( t, sample.zsumrev, marker='o', color=c )
            ax_sigz_rev.add_line( line )

            x = list( range( 8, 20 ) )
            try:
                dz = [ sample.zsums[-1] - sample.zsums[-n-1] - sample.zsums[n-1] for n in x ]
                if sample.confirmed:
                    dzs_confirm.extend( dz )
                    xs_confirm.extend( x )
                else:
                    dzs_unconfirm.extend( dz )
                    xs_unconfirm.extend( x )
            except IndexError as e:
                log.error( "zsums only has {} values".format( len( sample.zsums ) ) )

        for ax in ( ax_sigx, ax_sigx_rev, ax_sigy, ax_sigy_rev, ax_sigz, ax_sigz_rev ):
            ax.set_title( ax.get_title() + ": {} values".format( vals ) )
            ax.grid()
            ax.relim()
            ax.autoscale_view()

        fig = plt.figure()
        plt.title( "dz delta N" )
        plt.scatter( xs_unconfirm, dzs_unconfirm, color='r' )
        plt.scatter( xs_confirm, dzs_confirm, color='b' )
        plt.show()

    def _getMeasureMatrix( self, **kwargs ):
        return [ sample.measures for sample in self.filter_samples( **kwargs )
                if len( sample.measures ) == 96 ]
    measurematrix = property( fget=_getMeasureMatrix )

    @staticmethod
    def _parse_fifo( fname ):
     try:
       with open( fname, 'rb' ) as fh:
        binval = fh.read(4)

        """ find first start delimiter"""
        started = False
        while binval:
            if struct.unpack("<I", binval)[0] == 0xaaaaaaaa:
                started = True
                log.debug("Found start pattern 0xaaaaaaaa")
                break

            binval = fh.read(4)

        if not started:
            log.error("Could not find start patttern 0xaaaaaaaa in {}".format(fname))
            return []

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
        binval = fh.read(6)
        while binval:
            if len(binval) != 6:
                log.info("end of file encountered")
                break
            data = struct.unpack("<IBB", binval)
            end_unconfirm = (data[0] == 0xeeeeeeee)#FIFO end code
            end_confirm = (data[0] == 0xcccccccc) #FIFO end code
            if end_confirm or end_unconfirm:
                """end of fifo data"""
                log.debug("Found fifo end at 0x{:08x}".format(fh.tell()))

                if data[1:] == (0xdd, 0xee): #waketime log code
                    binval = fh.read(4)
                    waketime_ticks = struct.unpack("<I", binval)[0]
                    waketime_ms = waketime_ticks/TICKS_PER_MS
                    binval = fh.read(6) # update binval with next 6 bytes
                else:
                    """ waketime not include in this log dump"""
                    waketime_ms = 0
                    binval = binval[-2:] #retain last 2 bytes
                    binval += fh.read(4)

                ws = WakeSample( xs, ys, zs, waketime_ms, end_confirm, fname )
                samples.append(
                        WakeSample(xs, ys, zs, waketime_ms, end_confirm) )
                i+=1
                log.debug( "new sample: waketime={}ms confirmed={}".format(
                    waketime_ms, end_confirm ) )
                started = False

                xs = []
                ys = []
                zs = []

            if struct.unpack("<Ibb", binval)[0] == 0xdcdcdcdc:
                log.debug("Found DCLICK at 0x{:08x}".format(fh.tell()))
                """ retain last 2 bytes """
                binval = binval[-2:]
                binval += fh.read(4)

            if struct.unpack("<bbbbbb", binval)[2:3] == (0xdd, 0xba):
                log.debug("Found BAD FIFO at 0x{:08x}".format(fh.tell()))
                """ retain last 2 bytes """
                binval = binval[-2:]
                binval += fh.read(4)

            if started:
                (xh, xl, yh, yl, zh, zl) = struct.unpack("<bbbbbb", binval)
                xs.append(xl)
                ys.append(yl)
                zs.append(zl)

                binval = fh.read(6)
                log.debug("0x{:08x}  {}\t{}\t{}".format(fh.tell(), xl, yl, zl))

            else:
                """ Look for magic start code """
                if len(binval) != 6:
                    log.info("end of file encountered")
                    break

                if struct.unpack( "<Ibb", binval )[0] == 0xaaaaaaaa:
                    log.debug("Found start code 0x{:08x}".format(fh.tell()))
                    """ retain last 2 bytes """
                    binval = binval[-2:]
                    binval += fh.read(4)
                    started = True
                else:
                    log.debug("Skipping {} searching for start code".format(binval))
                    binval = fh.read(6)

            if len(binval) != 6:
                log.info("end of file encountered")
                break
            if struct.unpack("<Ibb", binval)[0] == 0xffffffff:
                log.debug("No more data after 0x{:08x}".format(fh.tell()))
                break
        log.debug("Parsed {} samples from {}".format(len(samples), fname))
        return samples
     except:
         log.error ("Unable to open file \'{}\'".format(fname))
         return []


class WakeSample( object ):
    _sample_counter = 0
    def __init__( self, xs, ys, zs, waketime=0, confirmed=False, logfile="" ):
        self.uid = uuid.uuid4().hex[-8:]
        self.logfile = logfile
        self.xs = xs
        self.ys = ys
        self.zs = zs
        self.waketime = waketime
        self.confirmed = confirmed
        WakeSample._sample_counter += 1
        self.i = WakeSample._sample_counter
        self._check_result = None
        self._collect_sums()

    def _getMeasures( self ): return self.xs + self.ys + self.zs
    measures = property( fget=_getMeasures )

    def _collect_sums( self ):
        self.xsums = [ sum( self.xs[:i+1] ) for i in range( len( self.xs ) ) ]
        self.ysums = [ sum( self.ys[:i+1] ) for i in range( len( self.ys ) ) ]
        self.zsums = [ sum( self.zs[:i+1] ) for i in range( len( self.zs ) ) ]
        self.xsumrev = [ sum( self.xs[-i-1:] ) for i in range( len( self.xs ) ) ]
        self.ysumrev = [ sum( self.ys[-i-1:] ) for i in range( len( self.ys ) ) ]
        self.zsumrev = [ sum( self.zs[-i-1:] ) for i in range( len( self.zs ) ) ]

    def _fltr_y_not_deliberate_fail( self ):
        return self.ys[-1] < -5

    def _fltr_y_turn_accept( self ):
        assert sum( self.ys[:9] ) == self.ysums[8]
        return abs( self.ysums[8] ) >= 240

    def _fltr_x_turn_accept( self ):
        assert sum( self.xs[:5] ) == self.xsums[4]
        return abs(self.xsums[4]) >= 120

    def _fltr_xy_turn_accept( self ):
        return abs( self.ysums[8] ) + abs( self.xsums[4] ) > 140

    def _fltr_z_sum_slope_accept( self ):
        assert sum( self.zs[-11:] ) == sum( self.zs[:-12:-1] )
        assert sum( self.zs[:11] ) == self.zsums[10]
        assert sum( self.zs[:5] ) == self.zsums[4]
        assert sum( self.zs[-11:] ) == self.zsums[31] - self.zsums[31-11]

        assert self.dzN == self.zsums[31] - self.zsums[31-11] - self.zsums[10]

        test1 = self.zsums[4] < 100
        test2 = self.zsums[31] - self.zsums[31-11] - self.zsums[10] > 110

        return test1 and test2

    def _fltr_y_inwards_accept( self ):
        return self.ys[-1] > 6

    def _fltr_y_overshoot_accept( self ):
        testsum1a = self.ysums[-1] - self.ysums[26]
        testsum1b = sum( self.ys[-5:] )
        assert( testsum1a == testsum1b )
        test1 = testsum1a > 20

        testsum2a = self.ysums[-1] - self.ysums[22]
        testsum2b = sum(self.ys[-9:])
        assert( testsum2a == testsum2b )
        test2 = testsum2a > 40
        return test1 or test2

    def wakeup_check( self, force=False ):
        if force:
            self._check_result = None
        if self._check_result is not None:
            return self._check_result
        elif len( self.xs ) < 1:
            self._check_result = False
            return self._check_result

        self.dzN = sum( self.zs[-11:] ) - sum( self.zs[:11] )
        self.ysum_n = self.ysums[8]
        self.xsum_n = self.xsums[4]

        if False: # to make it easier to re-arrange below tests
            pass
        elif self._fltr_y_turn_accept():
            self._check_result = True
        elif self._fltr_y_not_deliberate_fail():
            self._check_result = False
        elif self._fltr_xy_turn_accept():
            self._check_result = True
        elif self._fltr_x_turn_accept():
            self._check_result = True
        elif self._fltr_z_sum_slope_accept():
            self._check_result = True
        elif self._fltr_y_inwards_accept():
            self._check_result = True
        elif self._fltr_y_overshoot_accept():
            self._check_result = True
        else:
            self._check_result = False
        return self._check_result
    accepted = property( fget=wakeup_check )

    def is_false_negative(self):
        return not self.wakeup_check() and self.confirmed

    def sumN_score(self, n):
        xN = sum(self.xs[:n])
        yN = sum(self.ys[:n])
        zN = sum(self.zs[:n])
        dxN = sum(self.xs[:n:-1]) - xN
        dyN = sum(self.ys[:n:-1]) - yN
        dzN = sum(self.zs[:n:-1]) - zN

        #return abs(xN) + abs(yN) + abs(dxN) + abs(dyN) + dzN

        if self.ys[-1] < -10:
            return 0


        if abs(xN) > abs(yN):
            return abs(xN) + abs(dxN) + dzN
        else:
            return abs(yN) + abs(dyN) + dzN

    def show_plot( self ):
        fig = plt.figure()
        ax = fig.add_subplot( 111 )
        plt.title( "sample {} pass={} confirm={} xN={} yN={} dzN={}".format(
            self.uid, self.accepted, self.confirmed,
            self.xsum_n, self.ysum_n, self.dzN ) )
        ax.grid()
        t = range( len( self.xs ) )
        x_line = plt.Line2D( t, self.xs, color='r', marker='o' )
        y_line = plt.Line2D( t, self.ys, color = 'g', marker='o' )
        z_line = plt.Line2D( t, self.zs, color = 'b', marker='o' )
        ax.add_line( x_line )
        ax.add_line( y_line )
        ax.add_line( z_line )
        plt.figlegend( ( x_line, y_line, z_line ), ( "x", "y", "z" ),
                "upper right" )
        ax.set_xlim( [0, len(xs)] )
        ax.set_ylim( [-60, 60] )
        plt.xlabel( "1 / Output Data Rate" )
        plt.ylabel( "1/32 * g's for +/-4g" )
        plt.show()

    def export_csv( self ):
        with open('{}.csv'.format( self.uid ), 'wt') as csvfile:
            writer = csv.writer( csvfile, delimiter=' ' )
            for i, (x, y, z) in enumerate( zip( self.xs, self.ys, self.zs ) ):
                writer.writerow( [ i, x, y, z ] )



### Analysis function for streaming xyz data ###
def analyze_streamed( fname, plot=True ):
    try:
        with open( fname, 'rb' ) as fh:
            fh.seek(0x80) # skip usage data block
            binval = fh.read(4)
            #skip any leading 0xffffff bytes
            while struct.unpack("<I", binval)[0] == 0xffffffff:
                binval = fh.read(4)

            t = 0
            ts = []
            xs = []
            ys = []
            zs = []
            mag = []
            while binval:
                if struct.unpack ("<I", binval)[0] == 0xffffffff:
                    break

                ( z, y, x, dt ) = struct.unpack ( "<bbbB", binval )
                t += dt

                ts.append( t )
                xs.append( x )
                ys.append( y )
                zs.append( z )
                mag.append( ( x**2 + y**2 + z**2 )**0.5 )
                log.debug("{}\t{}\t{}\t{}".format(t, x, y, z))

                binval = fh.read(4)

            if plot:
                plt.plot(ts, xs, 'r-', label='x')
                plt.plot(ts, ys, 'g-', label='y')
                plt.plot(ts, zs, 'b-', label='z')
                plt.plot( ts, mag, 'k-', label="mag" )
                plt.legend()
                plt.show()
            return ts, xs, ys, zs
    except OSError as e:
        log.error( "Unable to open file '{}': {}".format(fname, e) )
        return [], [], [], []

### Filter tests functions ###
def make_traditional_tests():
    y_not_delib_fail = SampleTest( "Y not deliberate fail / Inwards accept",
            lambda s : s.ys[-1], reject_below=-5, accept_above=6 )

    y_turn_accept = SampleTest( "Y turn accept",
            lambda s : abs(s.ysums[8]), accept_above=240 )

    x_turn_accept = SampleTest( "X turn accept",
            lambda s : abs(s.xsums[4]), accept_above=120 )

    xy_turn_accept = SampleTest( "XY Turn Accept",
            lambda s : abs( s.ysums[8] ) + abs( s.xsums[4] ), accept_above=140 )

    y_ovs1_accept = SampleTest( "Y overshoot 1 accept",
            lambda s : s.ysums[-1] - s.ysums[26], accept_above=20 )

    y_ovs2_accept = SampleTest( "Y overshoot 2 accept",
            lambda s : s.ysums[-1] - s.ysums[22], accept_above=40 )

    z_sum_slope_accept = SampleTest( "Z slope sum accept",
            lambda s : ( s.zsums[4], s.zsums[31] - s.zsums[31-11] - s.zsums[10] ),
            accept_above=( None, 110 ), accept_below=( 100, None ) )

    fail_all_test = SampleTest( "Fail remaining", lambda s : -1, reject_below=0 )

    tests = [ y_turn_accept, y_not_delib_fail,
            y_ovs1_accept, xy_turn_accept, x_turn_accept, z_sum_slope_accept,
            y_ovs2_accept, fail_all_test ]

    return tests

def run_tests( tests, samples, plot=False ):
    for test in tests:
        test.clear_samples()

    for test in tests:
        test.add_samples( samples )
        test.analyze()
        test.show_result()
        if plot:
            test.plot_result()
        samples = test.passed_samples

### Linear algebra functions ###
def mean_center_columns( matrix ):
    """ matrix has the form
      [ [ ===  row 0  === ],
        [ ===  .....  === ],
        [ === row N-1 === ] ]
        we want each column to be mean centered
    """
    cms = get_col_means( matrix )
    mT = ( [ x - cm for x in c ] for cm, c in zip( cms, zip( *matrix )) )
    return list( zip( *mT ) )

def get_col_means( matrix ):
    N_inv = 1.0 / len( matrix )
    cms = ( sum( c ) * N_inv for c in zip( *matrix ) )
    return cms

def get_col_variances( matrix ):
    N_inv = 1.0 / len( matrix )
    vars = ( sum( x**2 for x in c ) * N_inv for c in zip( *matrix ) )
    return vars

def univarance_scale_columns( matrix ):
    vars = get_col_variances( matrix )
    mT = ( [ x / cv**0.5 for x in c ] for cv, c in zip( vars, zip( *matrix )) )
    return list( zip( *mT ) )

def find_weights( matrix, k=None ):
    xTx = np.dot( np.transpose( matrix ), matrix )
    evals, evect = np.linalg.eig( xTx )
    """ v should be zero by definition of eigenvalues / eigenvectors
        where A v = lambda v

        v_k is evect[:, k]
    v = [ a - b for a, b in zip( np.dot( xTx, evect[:,0] ), evals[0], evect[:,0] ) ]
    v should contain all zeros
    """
    weights = evect
    if k is None:
        return weights
    return [ w[k] for w in weights ]

### Relics ###
def plot_sumN_scores(samples):
    conf_scores = []
    unconf_scores = []
    for n in range(5, 20):
        conf_scores_n = []
        unconf_scores_n = []
        for s in samples:
            score = s.sumN_score(n)
            if s.confirmed: conf_scores_n.append(score)
            else: unconf_scores_n.append(score)
        conf_scores.append(conf_scores_n)
        unconf_scores.append(unconf_scores_n)

    for n in range(5, 20):
        conf = conf_scores[n-5]
        plt.plot(list(2*n-0.2 - 0.4*random.random() for i in range(len(conf))), conf, 'b.', label='confirmed')
        unconf = unconf_scores[n-5]
        plt.plot(list(2*n+0.2 + 0.4*random.random() for i in range(len(unconf))), unconf, 'r.', label='unconfirmed')

def plot_sums(samples, x_cnt = 5, y_cnt = 8):
    xsums = [sum(s.xs[:x_cnt]) for s in samples if s.confirmed]
    ysums = [sum(s.ys[:y_cnt]) for s in samples if s.confirmed]
    ssums = [abs(x) + abs(y) for x,y in zip(xsums, ysums)]

    xsums_u = [sum(s.xs[:x_cnt]) for s in samples if not s.confirmed]
    ysums_u = [sum(s.ys[:y_cnt]) for s in samples if not s.confirmed]
    ssums_u = [abs(x) + abs(y) for x,y in zip(xsums_u, ysums_u)]

    #plt.plot(xsums, color="r")
    plt.plot(ysums, marker=".", color="b")
    #plt.plot(ssums, color="g")

    #plt.plot(xsums_u, linestyle=":", color="r")
    #plt.plot(ssums_u, linestyle=":",  color="g")
    plt.plot(ysums_u, linestyle=":",  color="b")

if __name__ == "__main__":
    log.basicConfig( level = log.INFO )

    def parse_args(  ):
        parser = argparse.ArgumentParser(description='Analyze an accel log dump')
        parser.add_argument('dumpfiles', nargs='+')
        parser.add_argument('-f', '--fifo', action='store_true', default=True)
        parser.add_argument('-t', '--run-tests', action='store_true', default=True)
        parser.add_argument('-s', '--streamed', action='store_true', default=False)
        parser.add_argument('-w', '--export', action='store_true', default=False)
        parser.add_argument('-a', '--plot', action='store_true', default=False)
        parser.add_argument('-c', '--plot_csums', action='store_true', default=False)
        parser.add_argument('-r', '--rejects_only', action='store_true', default=False)
        parser.add_argument('-p', '--accepts_only', action='store_true', default=False)
        parser.add_argument('-fn', '--false_negatives', action='store_true', default=False)
        return parser.parse_args()

    args = parse_args()

    if not args.streamed and args.fifo:
        allsamples = Samples()
        sampleslist = []
        for fname in args.dumpfiles:
            newsamples = Samples()
            newsamples.load( fname )
            newsamples.analyze()
            if newsamples.total:
                sampleslist.append( newsamples )
                newsamples.show_analysis()
                allsamples.combine( newsamples )

        allsamples.show_analysis()

        if args.rejects_only:
            log.info( "showing rejects only" )
            kwargs = { "reject_only": True }
        elif args.false_negatives:
            log.info( "showing false_negatives only" )
            kwargs = { "false_negatives": True }
        elif args.accepts_only:
            log.info( "showing accepts only" )
            kwargs = { "accepts_only": True }
        else:
            kwargs = dict()

        if args.run_tests:
            traditional_tests = make_traditional_tests()
            run_tests( traditional_tests,
                    allsamples.filter_samples( **kwargs ), plot=args.plot )
        else:
            if args.plot:
                allsamples.show_plots( **kwargs )
        if args.export:
            allsamples.export_csv( **kwargs )
        if args.plot_csums:
            allsamples.show_csums( **kwargs )
    elif args.streamed:
        fname = args.dumpfiles[0]
        t, x, y, z = analyze_streamed( fname, plot=args.plot )


    #m = [ r[:10] for r in allsamples.measurematrix ]
    m = allsamples.measurematrix
    m_mc = mean_center_columns( m )
    m_uv = univarance_scale_columns( m_mc )
    w0 = find_weights( m_uv, 0 )
    w1 = find_weights( m_uv, 1 )
    w2 = find_weights( m_uv, 2 )

    def transform( weights, sample ):
        svals = sample.xs + sample.ys + sample.zs
        return sum( wi + si for wi, si in zip( weights, svals ) )

    def pctest( sample ):
        return transform( w0, sample ), transform( w1, sample ), transform( w2, sample )

    pc_test = SampleTest( "Principle components", pctest,
            accept_above=(0, None, None), accept_below=(None, 0, None),
            reject_below=(None, None, 0))

    pc0_test = SampleTest( "Principle component 0",
            lambda s: transform( w0, s ), accept_above=0 )
    pc1_test = SampleTest( "Principle component 1",
            lambda s: transform( w1, s ), accept_above=0 )
    pc2_test = SampleTest( "Principle component 2",
            lambda s: transform( w2, s ), accept_above=0 )

    for test in pc_test, pc0_test, pc1_test, pc2_test:
        test.clear_samples()
        test.add_samples( allsamples.samples )
        test.analyze()
        test.show_result()
        test.plot_result()
