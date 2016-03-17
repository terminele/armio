#!/usr/bin/python3

import os
import math
import time
import struct
import bisect
import argparse
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
import logging
import jsonpickle
import uuid
import csv
import random

log = logging.getLogger( __name__ )

TICKS_PER_MS = 1

SAMPLE_INT_MS = 10
rejecttime = 0
args = None
SLEEP_SAMPLE_PERIOD = 10        # ms, 100 Hz

REJECT, PASS, ACCEPT = range( 3 )

SAMPLESFILE=os.path.join(os.path.dirname(__file__), 'ALLSAMPLES.json')

np.set_printoptions(precision=6, linewidth=120)

class TestAttributes(object):
    def __init__(self):
        raise NotImplementedError("must be subclassed")

    def show_result( self, testsperday=None ):
        print("Analysis for '{}'".format( self.name ))
        print("{0:10}|{1:^20}|{2:^20}|{3:^12}".format("",
            "Confirmed", "Unconfirmed", "Totals"))
        print("{0:10}|{1:12,} :{4:>5} |{2:12,} :{5:>5} |{3:12,}".format("Accepted",
            self.confirmed_accepted, self.unconfirmed_accepted, self.accepted,
            '--' if self.true_positive is None else "{:.0%}".format(self.true_positive),
            '--' if self.false_positive is None else "{:.0%}".format(self.false_positive)))
        print("{0:10}|{1:12,} :{4:>5} |{2:12,} :{5:>5} |{3:12,}".format("Punted",
            self.confirmed_punted, self.unconfirmed_punted, self.punted,
            '--' if self.punted_positive is None else "{:.0%}".format(self.punted_positive),
            '--' if self.punted_negative is None else "{:.0%}".format(self.punted_negative)))
        print("{0:10}|{1:12,} :{4:>5} |{2:12,} :{5:>5} |{3:12,}".format("Rejected",
            self.confirmed_rejected, self.unconfirmed_rejected, self.rejected,
            '--' if self.false_negative is None else "{:.0%}".format(self.false_negative),
            '--' if self.true_negative is None else "{:.0%}".format(self.true_negative)))
        print("{0:10}|{1:12,} :{4:>5} |{2:12,} :{5:>5} |{3:12,}".format("Total",
            self.confirmed, self.unconfirmed, self.total, "100%", "100%"))
        if testsperday is not None and self.false_positive is not None:
            print("Accidental wakes per day: {:.0f}, from {} tests".format(
                testsperday*self.false_positive, testsperday))
        if self.unconfirmed_time_cnt > 0:
            print("Unconfirmed time analysis ({} values):".format(self.unconfirmed_time_cnt))
            print("  average unconfirmed time {:.1f} seconds".format(
                1e-3 * self.unconfirmed_time / self.unconfirmed_time_cnt))
            print("  maximum unconfirmed time {:.1f} seconds".format(
                1e-3 * self.unconfirmed_time_max))
            print("  {:.1f} of {:.1f} unconf sec accepted, {:.0%}".format(
                self.unconfirmed_accepted_time * 1e-3, self.unconfirmed_time * 1e-3,
                self.unconfirmed_accepted_time / self.unconfirmed_time))
            print("  {:.1f} of {:.1f} unconf sec punted, {:.0%}".format(
                self.unconfirmed_punted_time * 1e-3, self.unconfirmed_time * 1e-3,
                self.unconfirmed_punted_time / self.unconfirmed_time))
            print("  {:.1f} of {:.1f} unconf sec rejected, {:.0%}".format(
                self.unconfirmed_rejected_time * 1e-3, self.unconfirmed_time * 1e-3,
                self.unconfirmed_rejected_time / self.unconfirmed_time))
        print()

    def _getFalseNegative(self):
        if self.confirmed == 0:
            return None
        return self.confirmed_rejected / self.confirmed
    false_negative = property(fget=_getFalseNegative)

    def _getFalsePositive(self):
        if self.unconfirmed == 0:
            return None
        return self.unconfirmed_accepted / self.unconfirmed
    false_positive = property(fget=_getFalsePositive)

    def _getTrueNegative(self):
        if self.unconfirmed == 0:
            return None
        return self.unconfirmed_rejected / self.unconfirmed
    true_negative = property(fget=_getTrueNegative)

    def _getTruePositive(self):
        if self.confirmed == 0:
            return None
        return self.confirmed_accepted / self.confirmed
    true_positive = property(fget=_getTruePositive)

    def _getPuntedNegative(self):
        if self.unconfirmed == 0:
            return None
        return self.unconfirmed_punted / self.unconfirmed
    punted_negative = property(fget=_getPuntedNegative)

    def _getPuntedPositive(self):
        if self.confirmed == 0:
            return None
        return self.confirmed_punted / self.confirmed
    punted_positive = property(fget=_getPuntedPositive)


class MultiTest(TestAttributes):
    def __init__( self, *tests_and_samples, **kwargs ):
        self.name = kwargs.pop('name', "Combined Test Results")
        self.tests = []
        self.samples = []
        for item in tests_and_samples:
            if isinstance(item, TestAttributes):
                self.tests.append(item)
            elif isinstance(item, Samples):
                self.samples.extend(item.samples)
            else:
                for isub in item:
                    if isinstance(isub, TestAttributes):
                        self.tests.append(isub)
                    elif isinstance(isub, WakeSample):
                        self.samples.append(isub)
                    else:
                        raise ValueError("what is {}, type {}".format(
                            isub, type(isub)))
        self.clear_results()

    def clear_results(self):
        self.confirmed_accepted = 0
        self.confirmed_rejected = 0
        self.unconfirmed_accepted = 0
        self.unconfirmed_rejected = 0
        self.unconfirmed_time_cnt = None
        self.unconfirmed_time = None
        self.unconfirmed_time_max = None
        self.unconfirmed_accepted_time = 0
        self.unconfirmed_rejected_time = 0

    def run_tests(self, plot=False):
        self.clear_results()
        for test in self.tests:
            test.clear_samples()
        samples = self.samples
        for test in self.tests:
            test.add_samples( samples )
            test.analyze()
            test.show_result()
            self.confirmed_accepted += test.confirmed_accepted
            self.confirmed_rejected += test.confirmed_rejected
            self.unconfirmed_accepted += test.unconfirmed_accepted
            self.unconfirmed_rejected += test.unconfirmed_rejected
            self.unconfirmed_accepted_time += test.unconfirmed_accepted_time
            self.unconfirmed_rejected_time += test.unconfirmed_rejected_time
            if self.unconfirmed_time_cnt is None:
                self.unconfirmed_time = test.unconfirmed_time
                self.unconfirmed_time_cnt = test.unconfirmed_time_cnt
                self.unconfirmed_time_max = test.unconfirmed_time_max
            if plot:
                test.plot_result()
            samples = test.punted_samples

    def _getConfirmedPunted(self): return self.tests[-1].confirmed_punted
    confirmed_punted = property(fget=_getConfirmedPunted)

    def _getUnconfirmedPunted(self): return self.tests[-1].unconfirmed_punted
    unconfirmed_punted = property(fget=_getUnconfirmedPunted)

    def _getUnconfirmedPuntedTime(self): return self.tests[-1].unconfirmed_punted_time
    unconfirmed_punted_time = property(fget=_getUnconfirmedPuntedTime)

    def _getPuntedSamples(self): return self.tests[-1].punted_samples
    punted_samples = property(fget=_getPuntedSamples)

    def _getTotal(self): return self.confirmed + self.unconfirmed
    total = property(fget=_getTotal)

    def _getConfirmed(self): return self.confirmed_rejected + self.confirmed_accepted + self.confirmed_punted
    confirmed = property(fget=_getConfirmed)

    def _getUnconfirmed(self): return self.unconfirmed_rejected + self.unconfirmed_accepted + self.unconfirmed_punted
    unconfirmed = property(fget=_getUnconfirmed)

    def _getAccepted(self): return self.confirmed_accepted + self.unconfirmed_accepted
    accepted = property(fget=_getAccepted)

    def _getPunted(self): return self.confirmed_punted + self.unconfirmed_punted
    punted = property(fget=_getPunted)

    def _getRejected(self): return self.confirmed_rejected + self.unconfirmed_rejected
    rejected = property(fget=_getRejected)


class SampleTest(TestAttributes):
    def __init__( self, name, test_fcn, **kwargs ):
        """
            kwargs:
              test_names : optional individual names for multi-dimensional tests
        """
        self.name = name
        self._test_fcn = test_fcn
        self._reject_below = kwargs.pop( "reject_below", None )
        self._reject_above = kwargs.pop( "reject_above", None )
        self._accept_below = kwargs.pop( "accept_below", None )
        self._accept_above = kwargs.pop( "accept_above", None )
        self._test_names = kwargs.pop( "test_names", None )
        self.clear_samples()

    def clear_samples( self ):
        self.samples = []
        self.values = []
        self._confirmedvals = []
        self._unconfirmedvals = []
        self.total = 0
        self.confirmed = 0
        self.unconfirmed = 0
        self.unconfirmed_time = 0
        self.unconfirmed_time_cnt = 0
        self.unconfirmed_time_max = 0
        self.clear_results()

    def add_samples( self, *sample_sets ):
        for samples in sample_sets:
            samples_list = samples.samples if isinstance(samples, Samples) else samples
            for sample in samples_list:
                if len( sample.xs ) == 0:
                    continue
                self.total += 1
                val = self.test_fcn(sample)
                self.samples.append(sample)
                self.values.append(val)
                if sample.confirmed:
                    self.confirmed += 1
                    bisect.insort(self._confirmedvals, val)
                else:
                    self.unconfirmed += 1
                    bisect.insort(self._unconfirmedvals, val)
                    if sample.waketime:
                        self.unconfirmed_time_cnt += 1
                        self.unconfirmed_time += sample.waketime
                        if sample.waketime > self.unconfirmed_time_max:
                            self.unconfirmed_time_max = sample.waketime
                if self.analyzed:
                    self._record_test( sample, self._test_sample(val) )

    def _getTestFcn(self): return self._test_fcn
    def _setTestFcn(self, fcn):
        self._test_fcn = fcn
        self.retest()
    test_fcn = property(fget=_getTestFcn, fset=_setTestFcn)

    def retest(self):           # find all test values (no analysis)
        self.clear_results()
        self.values = []
        self._confirmedvals = []
        self._unconfirmedvals = []
        for s in self.samples:
            val = self.test_fcn(s)
            self.values.append(val)
            if s.confirmed:
                bisect.insort(self._confirmedvals, val)
            else:
                bisect.insort(self._unconfirmedvals, val)

    def _getMinConfirmed(self):
        if len(self.confirmedvals) == 0:
            return None
        return self.confirmedvals[0]
    minconfirmed = property(fget=_getMinConfirmed)

    def _getMinUnconfirmed(self):
        if len(self.unconfirmedvals) == 0:
            return None
        return self.unconfirmedvals[0]
    minunconfirmed = property(fget=_getMinUnconfirmed)

    def _getMaxConfirmed(self):
        if len(self.confirmedvals) == 0:
            return None
        return self.confirmedvals[-1]
    maxconfirmed = property(fget=_getMaxConfirmed)

    def _getMaxUnconfirmed(self):
        if len(self.unconfirmedvals) == 0:
            return None
        return self.unconfirmedvals[-1]
    maxunconfirmed = property(fget=_getMaxUnconfirmed)

    def _getMedianConfirmed(self):
        l = len(self.confirmedvals)
        if l % 2:
            return self.confirmedvals[l//2]
        else:
            i = l//2
            return 0.5 * (self.confirmedvals[i] + self.confirmedvals[i-1])
    midconfirmed = property(fget=_getMedianConfirmed)

    def _getMedianUnconfirmed(self):
        l = len(self.unconfirmedvals)
        if l % 2:
            return self.unconfirmedvals[l//2]
        else:
            i = l//2
            return 0.5 * (self.unconfirmedvals[i] + self.unconfirmedvals[i-1])
    midunconfirmed = property(fget=_getMedianUnconfirmed)

    def _getConfirmedValues(self): return self._confirmedvals
    confirmedvals = property(fget=_getConfirmedValues)

    def _getUnconfirmedValues(self): return self._unconfirmedvals
    unconfirmedvals = property(fget=_getUnconfirmedValues)

    def clear_results( self ):
        self.analyzed = False

        self._test_results = []
        self._punted_samples = []

        self._confirmed_accepted = None
        self._confirmed_rejected = None

        self._unconfirmed_accepted = None
        self._unconfirmed_rejected = None

        self._unconfirmed_accepted_time = 0
        self._unconfirmed_punted_time = 0
        self._unconfirmed_rejected_time = 0

    def _getRejectBelow(self): return self._reject_below
    def _setRejectBelow(self, val):
        if val != self._reject_below:
            self.clear_results()
            self._reject_below = val
    reject_below = property(fget=_getRejectBelow, fset=_setRejectBelow)

    def _getRejectAbove(self): return self._reject_above
    def _setRejectAbove(self, val):
        if val != self._reject_above:
            self.clear_results()
            self._reject_above = val
    reject_above = property(fget=_getRejectAbove, fset=_setRejectAbove)

    def _getAcceptBelow(self): return self._accept_below
    def _setAcceptBelow(self, val):
        if val != self._accept_below:
            self.clear_results()
            self._accept_below = val
    accept_below = property(fget=_getAcceptBelow, fset=_setAcceptBelow)

    def _getAcceptAbove(self): return self._accept_above
    def _setAcceptAbove(self, val):
        if val != self._accept_above:
            self.clear_results()
            self._accept_above = val
    accept_above = property(fget=_getAcceptAbove, fset=_setAcceptAbove)

    # accepted/punted/rejected and confirmed/unconfirmed represented as int
    def _getAccepted(self): return self.confirmed_accepted + self.unconfirmed_accepted
    accepted = property(fget=_getAccepted)

    def _getPunted(self): return self.confirmed_punted + self.unconfirmed_punted
    punted = property(fget=_getPunted)

    def _getRejected(self): return self.confirmed_rejected + self.unconfirmed_rejected
    rejected = property(fget=_getRejected)

    def _getConfirmedAccepted(self):
        if self._confirmed_accepted is None:
            self._confirmed_accepted = 0
            if len(self.confirmedvals) > 0:
                if isinstance(self.confirmedvals[0], (list, tuple)):
                    self.analyze()
                else:
                    if self.accept_above is not None:
                        self._confirmed_accepted += self.confirmed - bisect.bisect_right(self.confirmedvals, self.accept_above)
                    if self.accept_below is not None:
                        self._confirmed_accepted += bisect.bisect_left(self.confirmedvals, self.accept_below)
        return self._confirmed_accepted
    confirmed_accepted = property(fget=_getConfirmedAccepted)

    def _getConfirmedPunted(self): return self.confirmed - self.confirmed_accepted - self.confirmed_rejected
    confirmed_punted = property(fget=_getConfirmedPunted)

    def _getConfirmedRejected(self):
        if self._confirmed_rejected is None:
            self._confirmed_rejected = 0
            if len(self.confirmedvals) > 0:
                if isinstance(self.confirmedvals[0], (list, tuple)):
                    self.analyze()
                else:
                    if self.reject_above is not None:
                        self._confirmed_rejected += self.confirmed - bisect.bisect_right(self.confirmedvals, self.reject_above)
                    if self.reject_below is not None:
                        self._confirmed_rejected += bisect.bisect_left(self.confirmedvals, self.reject_below)
        return self._confirmed_rejected
    confirmed_rejected = property(fget=_getConfirmedRejected)

    def _getUnconfirmedAccepted(self):
        if self._unconfirmed_accepted is None:
            self._unconfirmed_accepted = 0
            if len(self.unconfirmedvals) > 0:
                if isinstance(self.unconfirmedvals[0], (list, tuple)):
                    self.analyze()
                else:
                    if self.accept_above is not None:
                        self._unconfirmed_accepted += self.unconfirmed - bisect.bisect_right(self.unconfirmedvals, self.accept_above)
                    if self.accept_below is not None:
                        self._unconfirmed_accepted += bisect.bisect_left(self.unconfirmedvals, self.accept_below)
        return self._unconfirmed_accepted
    unconfirmed_accepted = property(fget=_getUnconfirmedAccepted)

    def _getUnconfirmedPunted(self): return self.unconfirmed - self.unconfirmed_accepted - self.unconfirmed_rejected
    unconfirmed_punted = property(fget=_getUnconfirmedPunted)

    def _getUnconfirmedRejected(self):
        if self._unconfirmed_rejected is None:
            self._unconfirmed_rejected = 0
            if len(self.unconfirmedvals) > 0:
                if isinstance(self.unconfirmedvals[0], (list, tuple)):
                    self.analyze()
                else:
                    if self.reject_above is not None:
                        self._unconfirmed_rejected += self.unconfirmed - bisect.bisect_right(self.unconfirmedvals, self.reject_above)
                    if self.reject_below is not None:
                        self._unconfirmed_rejected += bisect.bisect_left(self.unconfirmedvals, self.reject_below)
        return self._unconfirmed_rejected
    unconfirmed_rejected = property(fget=_getUnconfirmedRejected)

    def set_thresholds(self, max_false_neg=0, max_false_pos=0):
        """ set thresholds based on acceptable false_neg / false_pos rates (%)
            max_false_neg -- allows the reject above / below thresholds to creep into confirmed sample space
            max_false_pos -- allows the accept above / below thresholds to creep into unconfirmed sample space
        """
        if self.confirmed == 0 or self.unconfirmed == 0:
            raise ValueError("We must have samples first")

        if max_false_neg > 0:
            if max_false_neg > 1:
                max_false_neg /= 100
            max_lost_confirm = int(max_false_neg * self.confirmed)

            min_rb_drop = bisect.bisect_left(self.unconfirmedvals, self.confirmedvals[0])
            max_rb_drop = bisect.bisect_left(self.unconfirmedvals, self.confirmedvals[max_lost_confirm])
            xtra_rb = max_rb_drop - min_rb_drop

            min_ra_drop = self.unconfirmed - bisect.bisect_right(self.unconfirmedvals, self.confirmedvals[-1])
            max_ra_drop = self.unconfirmed - bisect.bisect_right(self.unconfirmedvals, self.confirmedvals[-1-max_lost_confirm])
            xtra_ra = max_ra_drop - min_ra_drop

            if xtra_rb > xtra_ra:
                self.reject_above = self.confirmedvals[-1]
                self.reject_below = self.confirmedvals[max_lost_confirm]
            else:
                self.reject_above = self.confirmedvals[-1 - max_lost_confirm]
                self.reject_below = self.confirmedvals[0]
        else:
            self.reject_above = self.confirmedvals[-1]
            self.reject_below = self.confirmedvals[0]
        if max_false_pos > 0:
            if max_false_pos > 1:
                max_false_pos /= 100
            max_xtra_unconfirm = int(max_false_pos * self.unconfirmed)

            min_ab_keep = bisect.bisect_left(self.confirmedvals, self.unconfirmedvals[0])
            max_ab_keep = bisect.bisect_left(self.confirmedvals, self.unconfirmedvals[max_xtra_unconfirm])
            xtra_ab = max_ab_keep - min_ab_keep

            min_aa_keep = self.confirmed - bisect.bisect_right(self.confirmedvals, self.unconfirmedvals[-1])
            max_aa_keep = self.confirmed - bisect.bisect_right(self.confirmedvals, self.unconfirmedvals[-1-max_xtra_unconfirm])
            xtra_aa = max_aa_keep - min_aa_keep

            if xtra_ab > xtra_aa:
                self.accept_above = self.unconfirmedvals[-1]
                self.accept_below = self.unconfirmedvals[max_xtra_unconfirm]
            else:
                self.accept_above = self.unconfirmedvals[-1 - max_xtra_unconfirm]
                self.accept_below = self.unconfirmedvals[0]
        else:
            self.accept_above = self.unconfirmedvals[-1]
            self.accept_below = self.unconfirmedvals[0]

    def _getTestResults(self):
        if not self.analyzed:
            self.analyze()
        return self._test_results
    test_results = property(fget=_getTestResults)

    def _getPuntedSamples(self):
        if not self.analyzed:
            self.analyze()
        return self._punted_samples
    punted_samples = property( fget=_getPuntedSamples )

    def _getUnconfirmedAcceptedTime(self):
        if not self.analyzed:
            self.analyze()
        return self._unconfirmed_accepted_time
    unconfirmed_accepted_time = property(fget=_getUnconfirmedAcceptedTime)

    def _getUnconfirmedPuntedTime(self):
        if not self.analyzed:
            self.analyze()
        return self._unconfirmed_punted_time
    unconfirmed_punted_time = property(fget=_getUnconfirmedPuntedTime)

    def _getUnconfirmedRejectedTime(self):
        if not self.analyzed:
            self.analyze()
        return self._unconfirmed_rejected_time
    unconfirmed_rejected_time = property(fget=_getUnconfirmedRejectedTime)

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
            rb = self.reject_below
            ra = self.reject_above
            ab = self.accept_below
            aa = self.accept_above
        else:
            if self.reject_below is None:
                rb = None
            else:
                rb = self.reject_below[ fltr_num ]
            if self.reject_above is None:
                ra = None
            else:
                ra = self.reject_above[ fltr_num ]
            if self.accept_below is None:
                ab = None
            else:
                ab = self.accept_below[ fltr_num ]
            if self.accept_above is None:
                aa = None
            else:
                aa = self.accept_above[ fltr_num ]

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

    def _record_test( self, sample, testresult ):
        self._test_results.append( testresult )
        if testresult == PASS:
            self._punted_samples.append( sample )
        if sample.confirmed:
            if testresult == ACCEPT:
                self._confirmed_accepted += 1
            elif testresult == REJECT:
                self._confirmed_rejected += 1
        else:
            if testresult == ACCEPT:
                self._unconfirmed_accepted += 1
            elif testresult == REJECT:
                self._unconfirmed_rejected += 1

            if sample.waketime:
                if testresult == ACCEPT:
                    self._unconfirmed_accepted_time += sample.waketime
                elif testresult == REJECT:
                    self._unconfirmed_rejected_time += sample.waketime
                elif testresult == PASS:
                    self._unconfirmed_punted_time += sample.waketime

    def analyze( self ):
        self.clear_results()
        self._confirmed_accepted = 0
        self._confirmed_rejected = 0
        self._unconfirmed_accepted = 0
        self._unconfirmed_rejected = 0
        for val, sample in zip(self.values, self.samples):
            self._record_test( sample, self._test_sample(val) )
        self.analyzed = True

    def plot_result(self):
        ths = ( self.reject_below, self.reject_above,
                self.accept_below, self.accept_above )
        dims = set( len( th ) if isinstance( th, ( list, tuple ) ) else 1
                for th in ths if th is not None )
        if not len(self.values):
            raise ValueError("Nothing to plot")

        if len(dims) == 0:
            if isinstance(self.values[0], (list, tuple)):
                dim = len(self.values[0])
            else:
                dim = 1
        elif len(dims) == 1:
            dim = dims.pop()
        else:           # all tests should have same num dimensions
            raise ValueError( "{} has different dimensions {}".format(
                self.name, dims ) )
        if dim == 0:
            raise ValueError("Must have at least one dimensions")
        elif dim > 3:
            log.warning("Truncating to first 3 dimensions")

        wake_short = self.unconfirmed_time_max / 4
        wake_med = self.unconfirmed_time_max / 2

        CONFIRMED, SHORT, MED, LONG, TRAINSET = range( 5 )

        lines = dict()
        for tv in (PASS, ACCEPT, REJECT):
            for cv in (CONFIRMED, SHORT, MED, LONG, TRAINSET):
                lines[(tv, cv)] = []

        if 1 != len(set((len(self.test_results), len(self.samples), len(self.values)))):
            raise ValueError( "We are missing something {} {} {}".format(
                len(self.test_results), len(self.values), len(self.samples)))
        log.debug( "Plotting {} samples".format( len( self.values ) ) )

        tmin = min(s.timestamp for s in self.samples)
        for tv, val, sample in zip(self.test_results, self.values, self.samples):
            if getattr( sample, 'trainset', False ):
                cv = TRAINSET
            elif sample.confirmed:
                cv = CONFIRMED
            elif sample.waketime == 0:
                cv = MED
            elif sample.waketime < wake_short:
                cv = SHORT
            elif sample.waketime < wake_med:
                cv = MED
            else:
                cv = LONG
            if dim == 1:        # add an axis
                try:
                    t = sample.timestamp - tmin
                except TypeError:
                    t = sample.i
                val = ( t, val )
            elif dim > 2:       # truncate axes
                val = val[:3]
                val = ( val[2], val[0], val[1] )
            lines[ (tv, cv) ].append( val )

        fig = plt.figure()
        if dim == 1:
            ax_sigx = fig.add_subplot(111)
            ax_sigx.set_xlabel("Sample Number")
            ax_sigx.set_ylabel("Test Value")
        elif dim == 2:
            ax_sigx = fig.add_subplot(111)
            if self._test_names is not None:
                ax_sigx.set_xlabel(self._test_names[0])
                ax_sigx.set_ylabel(self._test_names[1])
            else:
                ax_sigx.set_xlabel("Test Value 0")
                ax_sigx.set_ylabel("Test Value 1")
        elif dim > 2:
            ax_sigx = fig.add_subplot(111, projection='3d')
            if self._test_names is not None:
                ax_sigx.set_xlabel(self._test_names[0])
                ax_sigx.set_ylabel(self._test_names[1])
                ax_sigx.set_zlabel(self._test_names[2])
            else:
                ax_sigx.set_xlabel("Test Value 0")
                ax_sigx.set_ylabel("Test Value 1")
                ax_sigx.set_zlabel("Test Value 2")

        ax_sigx.set_title(self.name)
        markers = ['x', '+', 'd']
        colors = ['green', 'yellow', 'orange', 'red', 'blue']

        for (test, confirm), data in lines.items():
            if not len(data):
                continue
            marker = markers[test]
            color = colors[confirm]
            if dim > 2:
                xs, ys, zs = list(zip(*data))
                plt.scatter( xs, ys, zs=zs, c=color, marker=marker )
            else:
                xs, ys = list(zip(*data))
                plt.scatter(xs, ys, color=color, marker=marker)

        ths = ( self.reject_below, self.reject_above,
                self.accept_below, self.accept_above )

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
            elif dim > 2:
                pass        # for now
        ax_sigx.autoscale( tight=True )

        plt.show()

    def plot_boxwhisker(self, dim=0):
        ths = ( self.reject_below, self.reject_above,
                self.accept_below, self.accept_above )
        dims = set( len( th ) if isinstance( th, ( list, tuple ) ) else 1
                for th in ths if th is not None )
        if not len(self.values):
            raise ValueError("Nothing to plot")

        if len(dims) == 0:
            if isinstance(self.values[0], (list, tuple)):
                dim = len(self.values[0])
            else:
                dim = 1
        elif len(dims) == 1:
            dim = dims.pop()
        else:           # all tests should have same num dimensions
            raise ValueError( "{} has different dimensions {}".format(
                self.name, dims ) )
        if dim == 0:
            raise ValueError("Must have at least one dimensions")
        elif dim > 3:
            log.warning("Truncating to first 3 dimensions")

        data = []
        labels = []
        if dim == 1:
            data.append([v for s, v in zip(self.samples, self.values) if s.confirmed])
            labels.append('confirmed')
            data.append([v for s, v in zip(self.samples, self.values) if not s.confirmed])
            labels.append('unconfirmed')
        else:
            for d in range(dim):
                labels.append('confirmed {}'.format(d))
                data.append([v[d] for s, v in zip(self.samples, self.values) if s.confirmed])
                labels.append('unconfirmed {}'.format(d))
                data.append([v[d] for s, v in zip(self.samples, self.values) if not s.confirmed])
        plt.boxplot(data, labels=labels)
        plt.show()

    def plot_accepts(self, resolution=100, start=None, stop=None):
        if start is None:
            start = min(v for v in (self.minunconfirmed, self.minconfirmed) if v is not None)
        if stop is None:
            stop = max(v for v in (self.maxunconfirmed, self.maxconfirmed) if v is not None)

        if isinstance( start, (tuple, list) ):
            raise NotImplementedError("Only 1 dimension allowed for now")

        thresholds = [v for v in np.linspace(start, stop, resolution)]

        if len(self.confirmedvals):
            vals = self.confirmedvals
            label = "Confirms"
            scale = 100.0 / len(vals)
            pct = [scale * bisect.bisect(vals, th) for th in thresholds]
            plt.plot(thresholds, pct, label=label)
        if len(self.unconfirmedvals):
            vals = self.unconfirmedvals
            label = "Unconfirms"
            scale = 100.0 / len(vals)
            pct = [scale * bisect.bisect(vals, th) for th in thresholds]
            plt.plot(thresholds, pct, label=label)

        ylim = [0, 100]
        xmin, xmax = plt.xlim()
        rb = self.reject_below
        ra = self.reject_above
        ab = self.accept_below
        aa = self.accept_above
        if ra is not None and ra >= xmin and ra <= xmax:
            plt.plot([ra, ra], ylim, 'r-.', label='reject_above')
        if rb is not None and rb >= xmin and rb <= xmax:
            plt.plot([rb, rb], ylim, 'r-', label='reject_below')
        if aa is not None and aa >= xmin and aa <= xmax:
            plt.plot([aa, aa], ylim, 'g-.', label='accept_above')
        if ab is not None and ab >= xmin and ab <= xmax:
            plt.plot([ab, ab], ylim, 'g-', label='accept_below')
        plt.legend()
        plt.grid()
        plt.ylim(ylim)
        plt.xlabel("Threshold")
        plt.ylabel("Percent Below Threshold (%)")
        plt.show()

    def filter_samples(self, **kwargs):
        return Samples.filter_samples(self.samples, **kwargs)


class PrincipalComponentTest( SampleTest ):
    _long_name = "Principal Component"
    _short_name = "PC"

    def __init__(self, *trainsets, **kwargs):
        """ use principal component analysis to find linear combinations
            of the accel data to test

            if trainsets is Samples instance, also add the samples

            kwargs:
                test_axis (0): single int index or list of index to select for test
                prereduce (None): pre-reduce matrix for reducing dimensions
        """
        self._reducedsamples = dict()
        if len(trainsets) == 0:
            """ This is a fixed weighting set (no training data) """
            name = kwargs.pop('name')
            weightings = kwargs.pop('weightings')
            test_fcn = lambda s : self.apply_weighting(weightings, s)
            self._prereduce = kwargs.pop("prereduce", None)
            self.eigvects = [weightings] + [[0]*len(weightings) for _ in range(len(weightings)-1)]
            self.eigvals = [1] + [0 for _ in range(len(weightings)-1)]
        else:
            self._prereduce = kwargs.pop("prereduce", None)
            if isinstance( self._prereduce, int ):
                pca = PrincipalComponentTest(*trainsets)
                self._prereduce = pca.getTransformationMatrix(self._prereduce)
            test_fcn = self._configure_test(*trainsets, **kwargs)
            defaultname = type(self)._long_name
            test_names = None
            if 'test_axis' in kwargs and isinstance(kwargs['test_axis'], int):
                defaultname += ' {}'.format(kwargs['test_axis'])
            elif 'test_axis' in kwargs:
                defaultname += 's {}'.format(tuple(kwargs['test_axis']))
                test_names = ["{} {}".format(type(self)._short_name, i) for i in kwargs['test_axis']]
            name = kwargs.pop('name', defaultname)
            kwargs.setdefault('test_names', test_names)
        self.trainsets = trainsets
        super().__init__(name, test_fcn, **kwargs)
        for ts in trainsets:
            if isinstance(ts, Samples):
                self.add_samples(ts.samples)
            else:
                self.add_samples(ts)

    def _find_weights_new(self, *datasets):
        matrix = []
        for ds in datasets:
            matrix.extend(ds.measurematrix if isinstance(ds, Samples) else ds)
        m = self.reduce_sample(matrix)
        n = len(m[0])
        M = np.array(m)
        means = np.mean(M, axis=0)
        mT = means.reshape(n, 1)
        S = np.zeros((n, n))
        for row in M:
            rT = row.reshape(n, 1)
            rT_mc = rT - mT
            S += rT_mc.dot(rT_mc.T)
        self.matrix = S
        self.getEigens(S)

    def _find_weights(self, *datasets):
        matrix = []
        for ds in datasets:
            if isinstance(ds, Samples):
                msub = ds.measurematrix
            elif isinstance(ds[0], WakeSample):
                msub = [s.measures for s in ds]
            else:
                msub = ds
            matrix.extend(msub)
        m = self.reduce_sample(matrix)
        """ mean center the columns """
        m = mean_center_columns(m)
        #m = self.univarance_scale_columns(m) if univarance_scale else m
        """ find the eigenvalues and eigenvectors of the matrix """
        xTx = np.dot(np.transpose(m), m)
        self.matrix = xTx
        self.getEigens(xTx)

    def _configure_test(self, *trainsets, **kwargs):
        self._find_weights(*trainsets)
        test_axis = kwargs.pop('test_axis', 0)
        if isinstance(test_axis, int):
            w = self.eigvects[test_axis]
            test_fcn = lambda s : self.apply_weighting(w, s)
        else:
            weights = [self.eigvects[i] for i in test_axis]
            test_fcn = lambda s : [self.apply_weighting(w, s) for w in weights]
        return test_fcn

    def getTransformationMatrix(self, num_axis=8):
        """ return the first <num_axis> values as a transformation matrix """
        evs = self.eigvects[:num_axis]
        tf = list(zip(*evs))
        return [list(r) for r in tf]

    def reduce_sample(self, sample):
        """ if we have a pre-reducer, use it to reduce dimensionality
            of original sample
        """
        if self._prereduce is None or not len(sample):
            return sample
        if not isinstance(sample, list) and sample in self._reducedsamples:
            return self._reducedsamples[sample]
        if isinstance( sample[0], (list, tuple) ):
            reduced = np.dot(np.array(sample), np.array(self._prereduce))
        else:
            # convert to a 'row' of a matrix and back again
            samplerow = np.array([sample])
            reduced = list(np.dot(samplerow, np.array(self._prereduce))[0])
        if not isinstance(sample, list):
            self._reducedsamples[sample] = reduced
        return reduced

    def apply_weighting(self, weights, sample):
        s_v = self.reduce_sample(tuple(sample.xs + sample.ys + sample.zs))
        return sum(wi * si for wi, si in zip(weights, s_v))

    @staticmethod
    def get_col_variances(matrix):
        N_inv = 1.0 / len(matrix)
        return (sum(x**2 for x in c) * N_inv for c in zip(*matrix))

    @classmethod
    def univarance_scale_columns(cls, matrix):
        vars = cls.get_col_variances(matrix)
        mT = ([x / cv**0.5 for x in c] for cv, c in zip(vars, zip(*matrix)))
        return list(zip(*mT))

    def getEigens(self, matrix):
        """ v should be zero by definition of eigenvalues / eigenvectors
            where A v = lambda v

            v_k is evect[:, k]
        v = [ a - b for a, b in zip( np.dot( xTx, evect[:,0] ), evals[0], evect[:,0] ) ]
        v should contain all zeros
        """
        evals, evect = np.linalg.eig(matrix)
        eva = [float(ev.real) for ev in evals]
        evv = [[float(r[i].real) for r in evect] for i in range(len(evals))]
        joined = (( ea, ev ) for ea, ev in zip(eva, evv))
        sort = sorted(joined, key=lambda k:k[0], reverse=True)
        eva, evv = list(zip(*sort))
        self.eigvals = eva
        self.eigvects = evv

    def show_xyz_filter(self, num=1):
        print('                        variance explained')
        eigv_cum = 0
        eigv_sum = sum(self.eigvals)
        for i, eigval in enumerate(self.eigvals):
            if num is not None and i >= num:
                break
            eigv_cum += eigval
            print('Eigenvalue{: 3},{: 9.2e}: {: 8.2%},{: 8.2%}'.format(i,
                eigval, (eigval/eigv_sum), (eigv_cum/eigv_sum)))

            vec, scale = self.get_xyz_weights(i)
            n = len(vec)//3
            xvals = vec[:n]
            yvals = vec[n:2*n]
            zvals = vec[2*n:]
            print("Scale is {:.3f}".format(scale))
            if self.reject_below is not None:
                print("static const int32_t rb_threshold = {:.0f}".format(scale*self.reject_below))
            if self.reject_above is not None:
                print("static const int32_t ra_threshold = {:.0f}".format(scale*self.reject_above))
            if self.accept_below is not None:
                print("static const int32_t ab_threshold = {:.0f}".format(scale*self.accept_below))
            if self.accept_above is not None:
                print("static const int32_t aa_threshold = {:.0f}".format(scale*self.accept_above))
            print("static const int32_t xfltr[] = {", end='\n    ')
            for j, f in enumerate(xvals):
                end = ', ' if (j + 1) % 8 else ',\n    '
                print( "{:6}".format(f), end=end )
            print( '};' )
            print("static const int32_t yfltr[] = {", end='\n    ')
            for j, f in enumerate(yvals):
                end = ', ' if (j + 1) % 8 else ',\n    '
                print( "{:6}".format(f), end=end )
            print( '};' )
            print("static const int32_t zfltr[] = {", end='\n    ')
            for j, f in enumerate(zvals):
                end = ', ' if (j + 1) % 8 else ',\n    '
                print( "{:6}".format(f), end=end )
            print( '};' )
            print()
            print('FixedWeightingTest([', end='\n    ')
            for j, f in enumerate(vec):
                end = ', ' if (j + 1) % 8 else ',\n    '
                print( "{:6}".format(f), end=end )
            print( ']', end='' )
            if self.reject_below is not None:
                print(', reject_below={:.0f}'.format(scale*self.reject_below), end='')
            if self.reject_above is not None:
                print(', reject_above={:.0f}'.format(scale*self.reject_above), end='')
            if self.accept_below is not None:
                print(', accept_below={:.0f}'.format(scale*self.accept_below), end='')
            if self.accept_above is not None:
                print(', accept_above={:.0f}'.format(scale*self.accept_above), end='')
            print( ')' )

    def get_xyz_weights(self, ndx=0, bits=17, scale=None):
        """ set bits so that sum product of 96 values that are 8-bit * bit fits
            96 < 128 (7 bits)
            accel size is 8 bit
            accel (int8_t) * maxsum (2**7) * bits (17) = 32
            2**7 == 128
        """
        if self._prereduce is not None:
            """ our eigenvalues span a smaller space """
            assert(len(self._prereduce[0]) == len(self.eigvals))
            prT = list(zip(*self._prereduce))
            assert(len(prT[0]) == 96)
            vec = [ 0 for _ in range(96) ]
            for w, pr_col in zip(self.eigvects[ndx], prT):
                for i in range(96):
                    vec[i] += w*pr_col[i]
        else:
            vec = self.eigvects[ndx]
        assert(len(vec)==96)
        if scale is None:
            vmax = max( abs(vi) for vi in vec )
            scale = 2**(bits - 1)/vmax
        vec = [ int(vi * scale) for vi in vec ]
        return vec, scale

    def show_eigvals(self, num=8, show_full_eig=False):
        if show_full_eig:
            for i in range(len(self.eigvals)):
                if num is not None and i >= num:
                    break
                print('Eigenvalue {}: {:.2e}'.format(i, self.eigvals[i]))
                print('Eigenvector {}: {}'.format(i, self.eigvects[i]), end='\n\n')

        print('                        variance explained')
        eigv_cum = 0
        eigv_sum = sum(self.eigvals)
        for i, eigval in enumerate(self.eigvals):
            if num is not None and i >= num:
                break
            eigv_cum += eigval
            print('Eigenvalue{: 3},{: 9.2e}: {: 8.2%},{: 8.2%}'.format(i,
                eigval, (eigval/eigv_sum), (eigv_cum/eigv_sum)))

    def plot_eigvals(self):
        fig = plt.figure()
        ax = fig.add_subplot(111)
        ax.set_xlabel("Principal Component Axis Number")
        ax.set_ylabel("Eigenvalue")
        ax.set_title("Eigenvalue Magnitude Analysis")
        eigs = plt.Line2D(list(range(len(self.eigvals))), self.eigvals,
                color='k', marker='d', linestyle=None)
        ax.add_line(eigs)
        ax.set_xlim([-0.1, 8.1])
        ax.set_ylim([0, max(self.eigvals) * 1.05])
        plt.show()

    def plot_weightings(self, ndx=0):
        fig = plt.figure()
        ax = fig.add_subplot(111)
        ax.set_xlabel("Time (sample num)")
        ax.set_ylabel("Scale Factor")
        ax.set_title("Scale factor for PCA index {}".format(ndx))
        wt, scale = self.get_xyz_weights(ndx)

        n = len(wt)//3
        x = wt[:n]
        y = wt[n:2*n]
        z = wt[2*n:]
        t = list(range(len(x)))
        xt = list(range(len(x)))
        yt = list(range(len(y)))
        zt = list(range(len(z)))
        m = [abs(xi) + abs(yi) + abs(zi) for xi, yi, zi in zip(x, y, z)]
        ax.add_line( plt.Line2D(t, m, color='k', label='mag') )
        ax.add_line( plt.Line2D(xt, x, color='r', label='x') )
        ax.add_line( plt.Line2D(yt, y, color='g', label='y') )
        ax.add_line( plt.Line2D(zt, z, color='b', label='z') )
        ax.relim()
        ax.autoscale_view()
        plt.legend()
        plt.show()

    def make_fixed_from_current(self, name=None):
        if name is None:
            name = "Fixed Weight Test"
        wts, scale = self.get_xyz_weights()
        fix = FixedWeightingTest(wts, name)
        fix.reject_above = self.reject_above * scale
        fix.reject_below = self.reject_below * scale
        fix.accept_above = self.accept_above * scale
        fix.accept_below = self.accept_below * scale
        return fix

    def check_updates(self, jump=1):
        wts = self.getWeightings()
        self.set_thresholds()
        stopped = False
        tn_std = self.true_negative
        tp_std = self.true_positive
        best_tn = (tn_std, wts)
        best_tp = (tp_std, wts)
        try:
            for i in range(len(wts)):
                best_neg = False
                best_pos = False
                newwts = list(wts)
                newwts[i] -= jump
                self.setWeightings(newwts)
                self.set_thresholds()
                tn_m1, tp_m1 = self.true_negative, self.true_positive
                if tn_m1 > best_tn[0]:
                    best_tn = (tn_m1, newwts)
                    best_neg = True
                if tp_m1 > best_tp[0]:
                    best_tp = (tp_m1, newwts)
                    best_pos = True
                newwts = list(wts)
                newwts[i] += jump
                self.setWeightings(newwts)
                self.set_thresholds()
                tn_p1, tp_p1 = self.true_negative, self.true_positive
                if tn_p1 > best_tn[0]:
                    best_tn = (tn_p1, newwts)
                    best_neg = True
                if tp_p1 > best_tp[0]:
                    best_tp = (tp_p1, newwts)
                    best_pos = True
                fmts = []
                for v in [tn_p1-tn_std, tn_m1-tn_std, tp_p1-tp_std, tp_m1-tp_std]:
                    if v > 0:
                        fmts.append("{:+6.1%}".format(v))
                    elif v == 0:
                        fmts.append("    0 ")
                    else:
                        fmts.append("   -- ")
                print("w[{:2}] = {:6.1f} +/-{:6.1f}: {:5.1%} ({} | {}) || {:5.1%} ({} | {}) {} {}".format(
                    i, wts[i], jump, tn_std, fmts[0], fmts[1], tp_std, fmts[2], fmts[3],
                    "<<" if best_neg else "  ", '<<' if best_pos else '  '))
        except KeyboardInterrupt:
            print("Stopped....")
            stopped = True
        finally:
            self.setWeightings(wts)
            self.set_thresholds()
        if stopped:
            raise KeyboardInterrupt("Terminated")
        return best_tn, best_tp

    def iterate(self, weights=0.9, startval=128, minval=1, maxiter=16, usenegative=True):
        if isinstance(weights, float) and weights < 1:
            wt = startval
            wlist = []
            iteration = 0
            while wt >= minval and iteration < maxiter:
                wlist.append(wt)
                wt = weights * wt
                iteration += 1
            weights = wlist
        for jp in weights:
            tn, tp = self.check_updates(jp)
            updated = list(tn[1] if usenegative else tp[1])
            self.setWeightings(updated)
            self.set_thresholds()

    def setWeightings(self, wts, **kwargs):
        changed = False
        try:
            pr = kwargs.pop("prereduce")
        except KeyError:
            pass
        else:
            if self._prereduce != pr:
                self._reducedsamples = dict()
                changed = True
                self._prereduce = pr
        if self._prereduce is not None:
            if isinstance(self._prereduce, int):
                if not len(self.samples):
                    raise ValueError("Cannot run PCA with no samples")
                pca = PrincipalComponentTest(self.samples)
                self._prereduce = pca.getTransformationMatrix(self._prereduce)
            dims = len(self._prereduce[0])
        else:
            dims = 96
        if wts is None:
            wts = [0] * dims
        if len(wts) != dims:
            raise ValueError("Must specify weight for all readings")

        old_wts = self.eigvects[0]
        if old_wts != wts:
            changed = False # re-setting test_fcn removes need to update
            test_fcn = lambda s : self.apply_weighting(wts, s)
            self.eigvects = [wts] + [[0]*len(wts) for _ in range(len(wts)-1)]
            self.eigvals = [1] + [0 for _ in range(len(wts)-1)]
            self.test_fcn = test_fcn
        if changed:
            self.retest()

    def getWeightings(self):
        return self.eigvects[0]


class FixedWeightingTest( PrincipalComponentTest ):
    def __init__(self, weightings, name, **kwargs):
        """ this should be used to 'capture' the results from a LDA / PCA test
        """
        if 'prereduce' in kwargs:
            dims = len(kwargs['prereduce'][0])
        else:
            dims = 96
        if len(weightings) != dims:
            raise ValueError("Must specify weight for all readings")
        kwargs.setdefault('weightings', weightings)
        kwargs.setdefault('name', name)
        kwargs.setdefault('test_axis', 0)
        super().__init__(**kwargs)


class SimpleOptimizer( FixedWeightingTest ):
    def __init__( self, **kwargs ):
        if 'prereduce' in kwargs:
            dims = len(kwargs['prereduce'][0])
        else:
            dims = 96
        wts = [0] * dims
        super().__init__(wts, "Optimizer Test", **kwargs)


class LinearDiscriminantTest( PrincipalComponentTest ):
    """ this analysis should create the scaling matricies
        based on the difference between our groups instead of
        just maximizing variance
    """
    _long_name = "Linear Discriminant Component"
    _short_name = "LDC"

    def _find_weights_new(self, *datasets):
        ds = []
        for dataset in datasets:
            if isinstance(dataset, Samples):
                if len(dataset.samples) == 0:
                    raise ValueError("{} has no samples".format(dataset.name))
                else:
                    log.info("{} has {} samples".format(dataset.name, len(dataset.samples)))
                data = dataset.measurematrix
            else:
                if len(dataset) == 0:
                    raise ValueError("No samples in a dataset")
                data = dataset
            ds.append(np.array(self.reduce_sample(data)))
        ns = [len(d) for d in ds]
        N_inv = 1.0 / sum(len(d) for d in ds)
        sum_all = np.sum([np.sum(d, axis=0) for d in ds], axis=0)
        mean_all = np.array([si*N_inv for si in sum_all])
        mean_groups = [np.mean(d, axis=0) for d in ds]

        dim = len(ds[0][0])
        mean_all = mean_all.reshape(dim, 1)
        mean_groups = [m.reshape(dim, 1) for m in mean_groups]

        # within class scatter
        S_W = np.zeros((dim, dim))
        for d, means in zip(ds, mean_groups):
            S = np.zeros((dim, dim))
            for row in d:
                rT = row.reshape(dim, 1)
                rT_mc = rT - means
                S += rT_mc.dot(rT_mc.T)
            S_W += S

        # between class scatter
        S_B = np.zeros((dim, dim))
        for n, means in zip(ns, mean_groups):
            S_B += n * (means - mean_all).dot((means - mean_all).T)
        SWinvSB = np.linalg.inv(S_W).dot(S_B)

        self.SW = S_W
        self.SB = S_B
        self.matrix = SWinvSB
        self.getEigens(SWinvSB)

    def _find_weights(self, *datasets):
        ds = []
        alldata = []
        for dataset in datasets:
            if isinstance(dataset, Samples):
                if len(dataset.samples) == 0:
                    raise ValueError("{} has no samples".format(dataset.name))
                data = dataset.measurematrix
                log.info("{} samples in {}".format(len(data), dataset.name))
            elif isinstance(dataset[0], WakeSample):
                if len(dataset) == 0:
                    raise ValueError("No samples in a dataset")
                data = [s.measures for s in dataset]
                log.info("{} samples in test set".format(len(data)))
            else:
                if len(dataset) == 0:
                    raise ValueError("No samples in a dataset")
                data = dataset
                log.info("{} samples in test set".format(len(data)))
            reduced = self.reduce_sample(data)
            ds.append(reduced)
            alldata.extend(reduced)

        # within class scatter
        dim = len(ds[0][0])
        S_W = np.zeros((dim, dim))
        for d in ds:
            m = mean_center_columns(d)
            xTx = np.dot(np.transpose(m), m)
            S_W += xTx

        # between class scatter
        S_B = np.zeros((dim, dim))
        mean_all = list(get_col_means(alldata))
        mean_all = np.array(mean_all).reshape(dim, 1)
        for d in ds:
            N = len(d)
            means = list(get_col_means(d))
            means = np.array(means).reshape(dim, 1)
            mdiff = [ ms - ma for ms, ma in zip(means, mean_all) ]
            S_B += N * np.dot(mdiff, np.transpose(mdiff))

        self.SW = S_W
        self.SB = S_B
        self.matrix = np.linalg.inv(S_W).dot(S_B)
        self.getEigens(self.matrix)


class LeastSquaresWeighting( FixedWeightingTest ):
    CONF_WEIGHT = 1e4
    def __init__( self, *trainsets, **kwargs ):
        if 'prereduce' in kwargs:
            self._prereduce = kwargs['prereduce']
        else:
            self._prereduce = None
        wts = self.do_least_sqare(*trainsets)
        super().__init__(wts, "Least Sqaares", **kwargs)
        for ts in trainsets:
            if isinstance(ts, Samples):
                self.add_samples(ts.samples)

    def do_least_sqare(self, *datasets):
        ds = []
        alldata = []
        weights = []
        for dataset in datasets:
            if isinstance(dataset, Samples):
                if len(dataset.samples) == 0:
                    raise ValueError("{} has no samples".format(dataset.name))
                else:
                    log.info("{} has {} samples".format(dataset.name, len(dataset.samples)))
                data = dataset.measurematrix
            else:
                if len(dataset) == 0:
                    raise ValueError("No samples in a dataset")
                data = dataset
            reduced = self.reduce_sample(data)
            ds.append(reduced)
            alldata.extend(reduced)
            for sample in dataset.samples:
                if isinstance( sample, WakeSample ):
                    if sample.confirmed:
                        weights.append(self.CONF_WEIGHT)
                    else:
                        weights.append(-1*sample.waketime)

        phi = list(zip(*alldata))
        Pinv = np.dot( phi, np.transpose( phi ) )
        B = np.dot( phi, weights )
        P = np.linalg.inv( Pinv )
        return [v for v in np.dot( P, B )]


class Samples( object ):
    def __init__( self, name=None ):
        self.samples = []
        self.battery_reads = []
        self.name = name

    def __setstate__(self, state):
        self.name = state['name']
        self.samples = state['samples']
        self.battery_reads = state.pop('batt', [])

    def __getstate__(self):
        state = dict()
        state['name'] = self.name
        state['samples'] = self.samples
        state['batt'] = self.battery_reads
        return state

    def _getMinTime(self):
        try:
            return min( s.timestamp for s in self.samples if s.timestamp is not None )
        except TypeError:
            return None
    mintime = property(fget=_getMinTime)

    def load( self, fn ):
        if self.name is None:
            self.name = os.path.basename(fn)
        newsamples, batt = self.parse_fifo( fn )
        self.samples.extend( newsamples )
        self.battery_reads.extend( batt )

    def combine( self, other ):
        if self.name is None:
            self.name = "Combined"
        self.samples.extend( other.samples )
        self.battery_reads.extend( other.battery_reads )

    def show_plots( self, **kwargs ):
        for sample in self.filter_samples( **kwargs ):
            sample.show_plot()

    def export_csv( self, **kwargs ):
        for sample in self.filter_samples( **kwargs ):
            sample.export_csv()

    def plot_z_for_groups( self, zmin=False, only='xymag' ):
        keys = sorted(list(set(s.logfile for s in self.samples)))
        groups = dict((k,[]) for k in keys)
        ncolors = len(keys)
        def span(low, high, num):
            if num == 1:
                space = int(high - low)
            else:
                space = int((high - low) / (num - 1))
            for i in range(num):
                yield i * space + low
        colors = ["#{:02X}{:02X}{:02X}".format(0xff-c, 0, c) for c in span(0, 0xff, ncolors)]
        cdict = dict((k, c) for k, c in zip(keys, colors))

        fig = plt.figure()
        ax = fig.add_subplot(111)
        if zmin:
            lines = dict((k, [59]*32) for k in keys)
            for sample in self.samples:
                l = lines[sample.logfile]
                for i in range(32):
                    if sample.zs[i] < lines[sample.logfile][i]:
                        lines[sample.logfile][i] = sample.zs[i]
            t = [SLEEP_SAMPLE_PERIOD*(i+0.5) for i in range(32)]
            for k in keys:
                ax.plot(t, lines[k], color=cdict[k], marker='.', linewidth=1, label=k)
            ax.legend()
        else:
            for sample in self.samples:
                if len(colors) > 1:
                    color=cdict[sample.logfile]
                else:
                    color=None
                sample.show_plot(only=only, color=color, show=False,
                        hide_legend=True, hide_title=True, axis=ax)
        ax.set_title("FIFO Plots showing {}".format(only))
        ax.grid()
        ax.set_xlim([0, SLEEP_SAMPLE_PERIOD*32])
        ax.set_ylim([-35, 35])
        ax.set_xlabel("ms (ODR = {} Hz)".format(1000/SLEEP_SAMPLE_PERIOD))
        ax.set_ylabel("1/32 * g's for +/-4g")
        plt.show()

    def getMeasureMatrix( self, **kwargs ):
        return get_measure_matrix( self.filter_samples(**kwargs) )
    measurematrix = property(fget=getMeasureMatrix)

    def find_fifo_log_start( self, filehandle ):
        """ find first start delimiter """
        START_CODE = (0x77, 0x77, 0x77) # close to max value, very unlikley
        BATT_START = (0x66, 0x66, 0x66)
        self.FIFO_START = START_CODE
        self.BATT_START = BATT_START
        self.start_found = None
        SC_STR = "0x" + ' '.join("{:02X}".format(s) for s in START_CODE)
        BSC_STR = "0x" + ' '.join("{:02X}".format(s) for s in BATT_START)
        matched_bytes = 0
        batt_startbytes = 0
        BLANK_BYTES = 0xFF
        MAX_BLANK_BYTES = 256
        MAX_SKIPPED_BYTES = 0x500
        skipped_bytes = 0
        blank_bytes = 0
        processed = 0
        while True:
            binval = filehandle.read(1)
            processed += 1
            if len(binval) < 1:
                log.error("End of file {}: could not find start code {}".format(
                    filehandle.name, SC_STR))
                return False
            startcode, = struct.unpack('<B', binval)
            if startcode == START_CODE[matched_bytes]:
                if batt_startbytes != 0:
                    log.warning("Discarding {} matched start code bytes".format(
                        batt_startbytes))
                    batt_startbytes = 0
                matched_bytes += 1
                if matched_bytes == len(START_CODE):
                    if processed > len(START_CODE):
                        log.info("Discarded {} values before start".format(
                            processed - len(START_CODE)))
                    log.debug("Found start code {}".format(SC_STR))
                    self.start_found = self.FIFO_START
                    return True
            elif startcode == BATT_START[batt_startbytes]:
                if matched_bytes != 0:
                    log.warning("Discarding {} matched start code bytes".format(
                        matched_bytes))
                    matched_bytes = 0
                batt_startbytes += 1
                if batt_startbytes == len(BATT_START):
                    if processed > len(BATT_START):
                        log.info("Discarded {} values before start".format(
                            processed - len(BATT_START)))
                    log.debug("Found battery start code {}".format(BSC_STR))
                    self.start_found = self.BATT_START
                    return True
            else:
                if matched_bytes != 0:
                    log.warning("Discarding {} matched start code bytes".format(
                        matched_bytes))
                    matched_bytes = 0
                if batt_startbytes != 0:
                    log.warning("Discarding {} matched start code bytes".format(
                        batt_startbytes))
                    batt_startbytes = 0
                if startcode != BLANK_BYTES:
                    blank_bytes = 0
                    log.debug("Skipping unknown byte: 0x{:02X}".format(startcode))
                elif blank_bytes >= MAX_BLANK_BYTES:
                    log.error("No start code {} after {} empty bytes".format(
                        SC_STR, MAX_BLANK_BYTES))
                    return False
                else:
                    blank_bytes += 1
                skipped_bytes += 1
                if skipped_bytes >= MAX_SKIPPED_BYTES:
                    log.error("No start code {} in {} after {} bytes".format(
                        SC_STR, filehandle.name, MAX_SKIPPED_BYTES))
                    return False

    @staticmethod
    def read_fifo_info( filehandle ):
        CONFIRM = 0xCC
        UNCONFIRM = 0xEE
        binval = filehandle.read(11)
        if len(binval) != 11:
            raise ValueError("End of the file.. binval came up short of 11 bytes")
        (confirmed, int1, int2, timestamp, waketicks) = struct.unpack(
                        '<BBBlL', binval)
        if confirmed not in { 0xEE, 0xCC }:
            raise ValueError("Confirmed bit should be 0xEE|0xCC, not 0x{:02X}".format(
                confirmed))
        confirm = ( confirmed == 0xCC )
        waketime_ms = waketicks / TICKS_PER_MS
        if timestamp > 1457519400:  # 10:30am march 3
            binval = filehandle.read(1)
            volt8, = struct.unpack('<B', binval)
            volt = (2048 + 4*volt8)/1024
        else:
            volt = None
        return confirm, int1, int2, timestamp, waketime_ms, volt

    @staticmethod
    def parse_fifo_log( filehandle, last_timestamp=0 ):
        END_CODE = (0x7F, 0x7F, 0x7F)
        try:
            confirm, int1, int2, timestamp, waketime, batt = Samples.read_fifo_info(filehandle)
        except ValueError as e:
            log.error("Error reading fifo info: {}".format(e))
            return None
        if timestamp < last_timestamp:
            log.error('Timestamp out of order!!!')
            return None
        elif timestamp == last_timestamp:
            log.warning("Duplicate timestamp at {}".format(_timestring(timestamp)))
        xs, ys, zs = [], [], []
        while True:
            binval = filehandle.read(3)
            if len( binval ) != 3:
                log.error("End of file encountered")
                return None
            data = struct.unpack("<" + "b"*3, binval)
            rawdata = struct.unpack('<' + 'B'*3, binval)
            if data == END_CODE:
                """ end of fifo data """
                log.debug("End code {} found".format('0x' + ' '.join(
                    '{:02X}'.format(d) for d in END_CODE)))
                if len(xs) and len(xs) == len(ys) and len(ys) == len(zs):
                    xtra = 32 - len(xs)
                    if xtra > 0:
                        log.debug("adding {} xtra values to sample".format(xtra))
                    for _ in range( xtra ):
                        xs.insert(0, None)
                        ys.insert(0, None)
                        zs.insert(0, None)
                    ws = WakeSample(xs, ys, zs,
                            waketime, confirm, filehandle.name,
                            timestamp, int1, int2, batt)
                    ws.logSummary()
                    return ws
                else:
                    log.info("Skipping invalid sample {}:{}:{}".format(
                        len(xs), len(ys), len(zs)))
                    return None

            log.debug("Adding sample {:2}: {:5} {:5} {:5} (0x{:02X} {:02X} {:02X})".format(
                len(xs), data[0], data[1], data[2], rawdata[0], rawdata[1], rawdata[2]))
            x, y, z = struct.unpack("<" + 'b'*3, binval)
            xs.append(x)
            ys.append(y)
            zs.append(z)
            if len(xs) > 32:
                log.warning("FIFO can't be longer than 32 samples, discarding")
                return None

    @staticmethod
    def parse_batt_sample(filehandle, last_timestamp=0):
        bv = filehandle.read(5)
        if len(bv) != 5:
            log.error("Error getting battery info. end of file?")
            return None
        timestamp, volt8 = struct.unpack('<LB', bv)
        volt = (2048 + 4*volt8)/1024
        if timestamp < last_timestamp:
            log.error("Backwards battery timestamp, skipping")
            return None
        elif timestamp == last_timestamp:
            log.warning("Duplicated battery timestamp")
        log.info('{}: BATTERY {:.3} V'.format(_timestring(timestamp), volt))
        return timestamp, volt

    def parse_fifo( self, fname ):
        """ parse the logfile (only look for fifo info),  little endian """
        samples = []
        battery_reads = []
        last_tstamp = 0
        with open(fname, 'rb') as fh:
            while(self.find_fifo_log_start(fh)):
                if self.start_found == self.FIFO_START:
                    ws = Samples.parse_fifo_log(fh, last_tstamp)
                    if ws is not None:
                        last_tstamp = ws.timestamp
                        samples.append(ws)
                elif self.start_found == self.BATT_START:
                    batt = self.parse_batt_sample(fh, last_tstamp)
                    if batt is not None:
                        last_timestamp = batt[0]
                        battery_reads.append(batt)
        log.info('Parsed {} samples, {} confirmed, {} battery reads'.format(
            len(samples), sum(1 for s in samples if s.confirmed), len(battery_reads)))
        return samples, battery_reads

    def show_wake_time_hist( self, samples=200 ):
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self

        times = [ s.waketime for s in sampleslist ]
        n, bins, patches = plt.hist(times, samples, normed=0.8, facecolor='green', alpha=0.5)

        plt.xlabel('Waketime (ms)')
        plt.ylabel('Probability')
        #plt.title(r'$\mathrm{Histogram\ of\ IQ:}\ \mu=100,\ \sigma=15$')
        #plt.axis([0, 30, 0, 0.5])
        plt.grid(True)

        plt.show()

    def get_wake_intervals(self, cutoff=None, **kwargs):
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self
        logfile = None
        lasttime = None
        for sample in Samples.filter_samples(sampleslist, **kwargs):
            if logfile is None:
                logfile = sample.logfile
            if logfile != sample.logfile:
                """ skip this sample """
                logfile = sample.logfile
                lasttime = None
                continue
            if lasttime is None:
                lasttime = sample.timestamp
                continue
            interval = sample.timestamp - lasttime
            lasttime = sample.timestamp
            if cutoff is None or interval <= cutoff:
                yield interval

    def group_wake_intervals(self, **kwargs):
        wi = list(Samples.get_wake_intervals(self, **kwargs))
        freq = dict()
        for item in wi:
            if item not in freq:
                freq[item] = 0
            freq[item] += 1
        return freq

    def _getWakeIntervalMedian(self):
        return np.median(list(Samples.get_wake_intervals(self, confirmed=False)))
    wake_interval = property(fget=_getWakeIntervalMedian)

    def getWakesPerDay(self):
        day_time_hrs = 16
        day_time_sec = 3600 * day_time_hrs
        interval = Samples._getWakeIntervalMedian(self)
        return int((day_time_sec + interval/2.0)/ interval)
    wake_tests_per_day = property(fget=getWakesPerDay)

    def show_wake_freq_hist(self, samples=200):
        if isinstance(self, Samples):
            wake_intervals = self.group_wake_intervals()
        else:
            wake_intervals = Samples.group_wake_intervals(self)
        total = sum(wake_intervals.values())
        intervals = sorted(wake_intervals.keys())
        percents = [(wake_intervals[i]/total * 100) for i in intervals]

        rm_highest_pct = 0.05
        rm_highest = int(rm_highest_pct*len(intervals))


        log.info("Interrupt interval median {:.1f} s".format(
            np.median(list(wake_intervals.values()))))
        plt.plot(intervals, percents, '.-')
        plt.xlim(0, intervals[-rm_highest])
        plt.xlabel('Time between samples (s)')
        plt.ylabel('Probability (%)')
        #plt.title(r'$\mathrm{Histogram\ of\ IQ:}\ \mu=100,\ \sigma=15$')
        #plt.axis([0, 500, 0, 0.03])
        plt.grid(True)
        plt.show()

    def plot_battery( self ):
        bvals = list(self.battery_reads)    # make a copy
        for s in self.samples:
            if s.batt is not None:
                bvals.append( (s.timestamp, s.batt) )
        sort_bv = sorted(bvals)
        times, batt_vals = list(zip(*bvals))
        t0 = times[0]
        gmt = time.gmtime(t0)
        starthour = gmt.tm_hour + gmt.tm_min/60.0 + gmt.tm_sec/3600.0
        scale = 1.0/3600
        t_h = [(ti-t0)*scale + starthour for ti in times]
        plt.plot(t_h, batt_vals, '.')
        plt.xlabel("Time of day (mod 24 hours)")
        plt.ylabel("Voltage (V)")
        plt.show()

    def find_outliers(self):
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self
        matrix = get_measure_matrix(sampleslist)
        mags = get_row_magnitudes(mean_center_columns(matrix))
        return sorted(zip(mags, sampleslist))

    def plot_outliers(self, skip=None):
        outliers = Samples.find_outliers(self)
        if skip is not None:
            outliers = outliers[:-skip]
        ovals, osamples = list(zip(*outliers))
        ovmin, ovscale = min(ovals), 0xff/(max(ovals) - min(ovals))
        ovscaled = [ int((o - ovmin)*ovscale) for o in ovals ]
        colors = [ "#{:02X}{:02X}{:02X}".format( o, 0, 0xff-o ) for o in ovscaled ]
        fig = plt.figure()

        ax = fig.add_subplot(311)
        for color, os in zip(colors, osamples):
            os.show_plot(axis=ax, color=color, only='x', show=False)
        ax = fig.add_subplot(312)
        for color, os in zip(colors, osamples):
            os.show_plot(axis=ax, color=color, only='y', show=False)
        ax = fig.add_subplot(313)
        for color, os in zip(colors, osamples):
            os.show_plot(axis=ax, color=color, only='z', show=False)
        plt.show()

    def remove_outliers(self, qty):
        """ in place removal of outlier samples """
        outs = Samples.find_outliers(self)[-qty:]
        removed = []
        for oval, out in outs:
            if isinstance(self, Samples):
                self.samples.remove(out)
            else:
                self.remove(out)
            removed.append(out)
        return removed

    def filter_samples(self, **kwargs):
        """
            show : display sample info
            reverse : reverse the test result
            or_tests : use testA or testB rather than testA and testB
            namehas : check if the name contains this string
            namenothas : check if the name does not contain this string

            any other kwarg will be tested against a property
        """
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self
        show = kwargs.pop( "show", False )      # show basic info
        reverse = kwargs.pop( "reverse", False )
        or_tests = kwargs.pop( "or_tests", False )

        debug_filter_result = len(kwargs) > 0
        namehas = kwargs.pop( "namehas", None )
        namenothas = kwargs.pop( "namenothas", None )

        def test( sample, filters ):
            for name, val in filters.items():
                yield getattr(sample, name) == val

        count = 0
        for sample in sampleslist:
            if or_tests:
                result = any(test(sample, kwargs))
                if namehas is not None:
                    result = result or (namehas in sample.logfile)
                if namenothas is not None:
                    result = result or (namenothas not in sample.logfile)
            else:
                result = all(test(sample, kwargs))
                if namehas is not None:
                    result = result and (namehas in sample.logfile)
                if namenothas is not None:
                    result = result and (namenothas not in sample.logfile)
            if reverse:
                result = not result
            if result:
                if show: sample.logSummary()
                count += 1
                yield sample
        if debug_filter_result:
            log.info("Found {} samples from {}".format(count, len(sampleslist)))

    def get_file_names(self):
        names = set()
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self
        for s in sampleslist:
            names.add(s.logfile)
        return sorted(names)


class WakeSample( object ):
    _sample_counter = 0
    def __init__(self, xs, ys, zs,
            waketime=0, confirmed=False, logfile="", timestamp=None,
            int1_flags=0xaa, int2_flags=0xaa, batt=None):
        self.uid = uuid.uuid4().hex[-8:]
        self.logfile = os.path.basename(logfile)
        self.xs = xs
        self.ys = ys
        self.zs = zs
        self.batt = batt
        self.waketime = waketime
        self.confirmed = confirmed
        self.timestamp = timestamp
        self.int1 = int1_flags
        self.int2 = int2_flags
        WakeSample._sample_counter += 1
        self.i = WakeSample._sample_counter
        self._check_result = None

    def __hash__(self):
        return hash(self.uid)

    def __setstate__(self, state):
        self.uid = state['uid']
        self.logfile = state['logfile']
        self.i = state['i']
        self.xs, self.ys, self.zs = state['accel']
        self.waketime = state['waketime']
        self.int1 = state["int1"]
        self.int2 = state["int2"]
        self.confirmed = state['confirmed']
        self.timestamp = state['timestamp']
        self.batt = state['batt']
        if self.i > WakeSample._sample_counter:
            WakeSample._sample_counter = i
        self._check_result = None

    def __getstate__(self):
        state = dict()
        state['uid'] = self.uid
        state['batt'] = self.batt
        state['logfile'] = self.logfile
        state['int1'] = self.int1
        state['int2'] = self.int2
        state['accel'] = self.xs, self.ys, self.zs
        state['waketime'] = self.waketime
        state['confirmed'] = self.confirmed
        state['timestamp'] = self.timestamp
        state['i'] = self.i
        return state

    def _getMeasures( self ): return self.xs + self.ys + self.zs
    measures = property( fget=_getMeasures )

    def _getIsSuperY(self): return bool(self.int2 & 0x80) and bool(self.int2 & 0x40)
    superY = property( fget=_getIsSuperY )

    def _getIsTriggerY(self): return bool(self.int2 & 0x40) and not self.superY
    triggerY = property( fget=_getIsTriggerY )

    def _getIsTriggerZ(self): return bool(self.int1 & 0x40) and not self.superY
    triggerZ = property( fget=_getIsTriggerZ )

    def _getIsFull(self):
        return not any(None in v for v in (self.xs, self.ys, self.zs))
    full = property(fget=_getIsFull)

    def _getSummary(self):
        return '{}: {:4.1f} sec, {:2} vals, {} V, {}'.format(
                _timestring(self.timestamp), self.waketime/1e3,
                sum(1 for x in self.xs if x is not None),
                '{:.2f}'.format(self.batt) if self.batt is not None else ' -- ',
                'CONFIRMED' if self.confirmed else 'unconfirmed')
    summary = property( fget=_getSummary )

    def _getInt1Summary(self):
        flags = ["sy", "ia", "zh", "zl", "yh", "yl", "xh", "xl"]
        i1f = (c == '1' for c in "{:08b}".format(self.int1))
        return 'int1:  '+ ' | '.join(
                "{:>2}".format(f if on else '') for f, on in zip(flags, i1f))
    tint1 = property( fget=_getInt1Summary )

    def _getInt2Summary(self):
        flags = ["sy", "ia", "zh", "zl", "yh", "yl", "xh", "xl"]
        i2f = (c == '1' for c in "{:08b}".format(self.int2))
        return 'int2:  '+ ' | '.join(
                "{:>2}".format(f if on else '') for f, on in zip(flags, i2f))
    tint2 = property( fget=_getInt2Summary )

    def logSummary(self):
        log.info(self.summary)
        log.info(self.tint1)
        log.info(self.tint2)

    def _collect_sums( self ):
        self.xsums = [ sum( v for v in self.xs[:i+1] if v is not None ) for i in range( len( self.xs ) ) ]
        self.ysums = [ sum( v for v in self.ys[:i+1] if v is not None ) for i in range( len( self.ys ) ) ]
        self.zsums = [ sum( v for v in self.zs[:i+1] if v is not None ) for i in range( len( self.zs ) ) ]
        self.xsumrev = [ sum( v for v in self.xs[-i-1:] if v is not None ) for i in range( len( self.xs ) ) ]
        self.ysumrev = [ sum( v for v in self.ys[-i-1:] if v is not None ) for i in range( len( self.ys ) ) ]
        self.zsumrev = [ sum( v for v in self.zs[-i-1:] if v is not None ) for i in range( len( self.zs ) ) ]

    def show_plot( self, **kwargs ):
        """ kwargs include:
            only ( None ): 'x|y|z|xymag|zxmag|zymag|xyzmag'
            color ( None ) : line color -- used only when 'only' is specified
            show ( True ) : if not to show, also suppresses labels
            hide_legend ( False ): hide the legend
            hide_title ( False ): hide the title
            axis (None) : optionally provide the axis to plot on
        """
        ax = kwargs.pop('axis', None)
        if ax is None:
            fig = plt.figure()
            ax = fig.add_subplot(111)
        t = [SLEEP_SAMPLE_PERIOD*(i+0.5) for i in range(len(self.xs))]
        only = kwargs.pop('only', None)
        if only is None:
            xym = [ (x**2 + y**2)**0.5 if (x is not None and y is not None) else None for x, y in zip(self.xs, self.ys) ]
            xzm = [ (x**2 + z**2)**0.5 if (x is not None and z is not None) else None for x, z in zip(self.xs, self.zs) ]
            yzm = [ (y**2 + z**2)**0.5 if (y is not None and z is not None) else None for y, z in zip(self.ys, self.zs) ]
            mag = [ (x**2 + y**2 + z**2)**0.5 if (x is not None and y is not None and z is not None) else None for x, y, z in zip(self.xs, self.ys, self.zs) ]
            ax.plot(t, self.xs, color='r', marker='.', label='x', linewidth=1)
            ax.plot(t, self.ys, color='g', marker='.', label='y', linewidth=1)
            ax.plot(t, self.zs, color='b', marker='.', label='z', linewidth=1)
            ax.plot(t, mag, color='k', marker='.', label='mag', linewidth=1, linestyle='-')
            ax.plot(t, xym, color='c', marker='.', label='xy', linewidth=1, linestyle=':')
            ax.plot(t, xzm, color='m', marker='.', label='xz', linewidth=1, linestyle=':')
            ax.plot(t, yzm, color='y', marker='.', label='yz', linewidth=1, linestyle=':')
        else:
            color = kwargs.pop('color', None)
            only = only.split(',')
            if color is None:
                color = ['r', 'g', 'b', 'c', 'm', 'y', 'k']
            else:
                color = [color]*7
            if 'x' in only: # red
                ax.plot(t, self.xs, color=color[0],
                        marker='.', label='z', linewidth=1)
            if 'y' in only: # green
                ax.plot(t, self.ys, color=color[1],
                        marker='.', label='z', linewidth=1)
            if 'z' in only: # blue
                ax.plot(t, self.zs, color=color[2],
                        marker='.', label='z', linewidth=1)
            if 'xyzmag' in only or 'mag' in only:   # black
                m = [ (x**2 + y**2 + z**2)**0.5 if (x is not None and y is not None and z is not None) else None for x, y, z in zip(self.xs, self.ys, self.zs) ]
                ax.plot(t, m, color=color[6],
                        marker='.', label='z', linewidth=1, linestyle='-')
            if 'xymag' in only or 'yxmag' in only:  # cyan
                m = [ (x**2 + y**2)**0.5 if (x is not None and y is not None) else None for x, y in zip(self.xs, self.ys) ]
                ax.plot(t, m, color=color[3],
                        marker='.', label='z', linewidth=1, linestyle=':')
            if 'xzmag' in only or 'zxmag' in only:  # magenta
                m = [ (x**2 + z**2)**0.5 if (x is not None and z is not None) else None for x, z in zip(self.xs, self.zs) ]
                ax.plot(t, m, color=color[4],
                        marker='.', label='z', linewidth=1, linestyle=':')
            if 'yzmag' in only or 'zymag' in only:  # yellow
                m = [ (y**2 + z**2)**0.5 if (y is not None and z is not None) else None for y, z in zip(self.ys, self.zs) ]
                ax.plot(t, m, color=color[5],
                        marker='.', label='z', linewidth=1, linestyle=':')
        if kwargs.pop( "show", True ):
            if not kwargs.pop( 'hide_title', False ):
                ax.set_title("sample {} confirm={}".format(self.uid, self.confirmed))
            ax.grid()
            ax.set_xlim([0, SLEEP_SAMPLE_PERIOD*(len(self.xs))])
            ax.set_ylim([-40, 40])
            if not kwargs.pop( 'hide_legend', False ):
                ax.legend(loc="upper right")
            ax.set_xlabel("ms (ODR = {} Hz)".format(1000/SLEEP_SAMPLE_PERIOD))
            ax.set_ylabel("1/32 * g's for +/-4g")
            plt.legend(loc="lower center").draggable()
            plt.show()      # ax.figure.show() would show all at once

    def export_csv( self ):
        with open('{}.csv'.format( self.uid ), 'wt') as csvfile:
            writer = csv.writer( csvfile, delimiter=' ' )
            for i, (x, y, z) in enumerate( zip( self.xs, self.ys, self.zs ) ):
                writer.writerow( [ i, x, y, z ] )


### HELPER FUNCTIONS ###
def _timestring( t=None ):
    if t is None:
        t = time.time()
    local = time.gmtime(t)
    fmt = "%Y-%m-%d %H:%M:%S"
    return time.strftime(fmt, local)

def get_col_means(matrix):
    N_inv = 1.0 / len(matrix)
    cms = (sum(c) * N_inv for c in zip(*matrix))
    return cms

def mean_center_columns(matrix):
    """ matrix has the form
      [ [ ===  row 0  === ],
        [ ===  .....  === ],
        [ === row N-1 === ] ]
        we want each column to be mean centered
    """
    cms = get_col_means(matrix)
    mT = ([x - cm for x in c] for cm, c in zip(cms, zip(*matrix)))
    return list(zip(*mT))

def get_row_magnitudes(matrix):
    for row in matrix:
        yield sum(ri**2 for ri in row)**0.5

def get_measure_matrix(self):
    if isinstance(self, Samples):
        return [sample.measures for sample in self.samples]
    else:
        return [sample.measures for sample in self]

def load_samples(samplefile=SAMPLESFILE):
    with open(samplefile, 'r') as fh:
        samplestext = fh.read()
    return jsonpickle.decode(samplestext)

def store_samples(samples, samplefile=SAMPLESFILE):
    samplestext = jsonpickle.encode(samples, keys=True)
    with open(samplefile, 'w') as fh:
        fh.write(samplestext)


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
            nvals = 0
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
                nvals += 1

                binval = fh.read(4)
            log.info("read {} values".format(nvals))

            if plot:
                plt.plot(ts, xs, 'r-', label='x')
                plt.plot(ts, ys, 'g-', label='y')
                plt.plot(ts, zs, 'b-', label='z')
                plt.plot(ts, mag, 'k-', label="mag")
                plt.legend().draggable()
                plt.show()
            else:
                log.info("Suppressing plot... use '--plot' option")
            return ts, xs, ys, zs
    except OSError as e:
        log.error( "Unable to open file '{}': {}".format(fname, e) )
        return [], [], [], []


### Filter tests functions ###
def make_traditional_tests():
    make_sums = SampleTest( "Do nothing, config sample",
            lambda s : 0 if s._collect_sums() is None else -1 )

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

    tests = [ make_sums, y_turn_accept, y_not_delib_fail,
            y_ovs1_accept, xy_turn_accept, x_turn_accept, z_sum_slope_accept,
            y_ovs2_accept, fail_all_test ]

    return tests

def make_LD_PCA_tests():
    y_turn_basic = FixedWeightingTest([
        -40818, -39279, -38761, -37612, -35009, -32590, -31453, -28327,
        -25222, -22015, -18545, -13371,  -7730,  -3216,     69,   4641,
          7749,  10355,  11959,  13870,  16961,  18592,  20119,  21121,
         21718,  22923,  23782,  24230,  24623,  24528,  24704,  24769,
        -45159, -41684, -39780, -36514, -36005, -31385, -22511, -18355,
        -12478,  -7202,  -2285,   1180,   9471,  14850,  19927,  31244,
         35899,  34538,  35525,  36231,  37540,  38461,  39855,  39691,
         39868,  40953,  41877,  42167,  42205,  42529,  42605,  43350,
        -65536, -64635, -61699, -60714, -59365, -55997, -49054, -39708,
        -31993, -25013, -18650, -15220, -12007,  -9281,  -3691,   -441,
          2218,   3906,   4633,   4328,   3845,   3662,   3985,   4080,
          3890,   3812,   3683,   3873,   3981,   4254,   4271,   3988,
        ], name="Basic y-turn test", accept_below=-28 )
    tests = [ y_turn_basic ]
    return tests


### mini scripts ###
def show_various_reductions(*samples, **kwargs):
    pca_test = kwargs.pop( 'pca_test', None )
    pcadims = kwargs.pop( 'pcadims', None )
    wake_tests_per_day = kwargs.pop( 'wake_tests_per_day', 3200 )
    if pca_test is None:
        pca_test = PrincipalComponentTest( *samples )
    if pcadims is None:
        pcadims = range(4, 16)
    for i in pcadims:
        ld = LinearDiscriminantTest(*samples, test_axis=0,
                prereduce=pca_test.getTransformationMatrix(i),
                accept_below=0, reject_above=0)
        ld.show_result(wake_tests_per_day)
        ld.plot_weightings()

def show_polar( yth=None, zth=None ):
    ax = plt.subplot(111, projection='polar')
    if yth is not None:
        theta_start = math.asin(yth/32)
        theta_end = math.pi - theta_start
        theta = np.arange(theta_start, theta_end, 0.01)
        r = [ 0.8 for _ in theta ]
        ax.plot(theta, r, color='r', linewidth=3, label='y level')
    if zth is not None:
        theta_start = math.pi - math.acos(zth/32)
        theta_end = math.pi + math.acos(zth/32)
        theta = np.arange(theta_start, theta_end, 0.01)
        r = [ 1.2 for _ in theta ]
        ax.plot(theta, r, color='g', linewidth=3, label="Z level")
    ax.set_rmax(1.5)
    plt.show()

def show_threshold_values():
    print("mag:  {:>5}  {:>5}".format("ang", "ths"))
    for th in range(33):
        angle = math.asin(th/32.0)*180/math.pi
        opposite = (32**2-th**2)**0.5
        print("{: 3}:  {:5.1f}  {:5.1f}".format(th, angle, opposite))

def make_ytrigger_tests():
    fw_1r = FixedWeightingTest([    # 41 % TN
             0,      0,      0,      0,      0,      0,      0,   8020,
             0,  -1002,      0,      0,      0,  -4010,      0,      0,
             0,      0,      0,  65536,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
         -2005,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0, -64160],
        "FW Reject 1 (41%)", reject_above=-400653)

    fw_2r = FixedWeightingTest([    # 20 % TN
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0, -20565,  18509,
        -58982,      0,      0,      0,      0,      0,  22850,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0, -13493,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,  16658, -65536],
        "FW Reject 2, (20%)", reject_below=-2315263, reject_above=-407864)

    fw8_1r = FixedWeightingTest([   # 43 % TN
         50095,  49230,  44934,  41586,  36390,  32490,  23022,  18562,
         15540,  12948,   8264,   1278,  -1525,  -5405,  -8214, -10230,
        -10826, -10193, -11050, -11633, -11377,  -9730,  -5378,  -3500,
         -2639,  -1404,   -425,   -899,  -1303,  -1528,   -494,  -1209,
         17572,  18145,  17664,  18904,  16473,  16721,  16675,  18888,
         16333,   7710,   7316,   4228,   9220,   3660,  -5305, -16028,
        -23254, -29881, -31137, -28879, -23603, -14884,  -2323,   1278,
          5327,   8516,  11649,  14693,  19098,  22100,  25634,  24566,
        -37982, -38856, -40333, -35939, -34252, -24964, -17757,  -6517,
          6075,  16444,  21522,  27203,  33066,  35491,  41283,  43955,
         39699,  34438,  24185,  10010,  -2529, -14671, -28332, -33729,
        -41748, -51015, -59745, -64036, -65536, -64836, -62490, -57876],
        "PCA8, Reject Test 1 (43%)", reject_below=-25070329, reject_above=-688153)

    fw8_2r = FixedWeightingTest([   # 18 % TN
         65536,  62751,  56627,  48966,  40793,  28216,  25064,  16176,
          7184,  -2558,  -4378,  -4104,  -3411,  -7848, -10309, -10180,
         -9951,  -7706,  -3744,  -2170,   2244,   5170,   7225,   7759,
          8456,   7746,   7535,   6080,   4253,   3336,   2439,   1818,
         -1636,  -6086,  -8843, -13564, -13515, -12555,  -5193,   -435,
         12681,  17362,  20278,  23838,  22370,  19226,  15548,   8085,
           376,  -5214,  -6262,  -8473,  -8060,  -7385,  -6610,  -8184,
        -10953, -12018, -11042,  -8230,  -6675,  -1839,    930,   2278,
         30243,  28864,  26357,  22162,  14133,  12338,   3177,  -5529,
         -7299,  -8062, -12386, -16814, -22460, -21927, -24546, -27166,
        -28510, -29612, -28337, -28857, -29392, -31143, -33239, -35479,
        -36630, -36807, -36765, -36500, -35461, -34917, -35890, -36099],
         "PCA8, Reject Test 2 (18%)", reject_below=-32937530, reject_above=-5282994)

    fw16_1r = FixedWeightingTest([  # 46 % TN
         65536,  62347,  53541,  45638,  37751,  28980,  24749,  19029,
         12767,   4639,  -3290,  -6418, -10881, -12138, -16782, -20190,
        -21818, -21587, -17923, -15273, -12000, -10005,  -5520,  -3493,
         -2319,  -1835,  -1420,  -1684,  -2506,  -2290,  -2609,  -2701,
          9992,   5852,   -454,  -7608,  -2396,   2150,  12303,  19971,
         32888,  29977,  23576,  13931,   7578,   2685,  -4008, -10033,
        -14365, -17588, -14672, -11754,  -9598,  -9390,  -6360,  -5505,
         -9084, -11451, -13160, -12699, -12178, -10683, -10488,  -9287,
         27200,  26243,  29002,  23549,  13754,   4435, -13782, -26828,
        -28343, -25528, -24327, -19538, -15619,  -9079,  -5139,  -2286,
          1684,   4376,   2771,  -2177,  -4724,  -7652, -11579, -14550,
        -16150, -17059, -17470, -17933, -18253, -17756, -18766, -20242],
        "PCA16, Reject Test 1 (46%)", reject_below=-24967540, reject_above=-2569097)

    fw16_2r = FixedWeightingTest([  # 25 % TN
         42377,  44939,  42218,  40454,  40961,  37860,  38976,  33860,
         28986,  23798,  15965,  14063,   9499,    475,  -6949, -17441,
        -25087, -29208, -29177, -29209, -27985, -27106, -22517, -19511,
        -16827, -16218, -16518, -15771, -16612, -16683, -15532, -15595,
          9063,   5845,  -4003,  -9563, -11355,  -7240,  -4617,   -707,
         38362,  53919,  52728,  43867,  30893,  11766,  -6853, -21610,
        -22567, -15307,  -2943,   9869,  18882,  17006,  18639,  14146,
          7073,   2023,   -629,  -1747,  -2542,   1369,   3569,   3504,
        -34633, -33380, -32787, -29418, -28300, -25140, -25692, -20748,
         -5335,   7690,  16851,  18129,  27346,  31201,  28353,  17762,
           254, -16829, -29653, -42398, -50543, -56262, -65536, -65320,
        -63078, -60978, -60395, -55036, -49823, -45944, -45826, -39175],
        "PCA16, Reject Test 2 (25%)", reject_below=-27370534, reject_above=-5433429)

    xturn = FixedWeightingTest([    #  9 % FP
         65536,  57569,  45895,  34267,  23752,  12529,   3780,  -2299,
         -6098,  -9472, -12935, -15495, -15535, -15643, -13070, -10558,
         -9886,  -4919,    -93,   2962,   6869,  11640,  12878,  13819,
         14303,  13735,  12892,  12451,  11024,   9764,   8751,   6313,
         23295,  17449,  10830,   6514,   1612,    -22,  -8133,  -7721,
         -2262,  -2712,  -5238, -10037, -11120, -10618, -11761,  -7046,
         -3318,   -939,   1249,   3562,   7231,   9062,  14608,  17889,
         19058,  19017,  20085,  20338,  21620,  22326,  21988,  24245,
          3736,   1292,   -255,   -947,  -6412,  -5342,  -4395,  -2770,
         -1193,   4409,   5394,   8624,   8067,   8471,   8375,   5719,
          4466,   2354,   1168,   -931,  -3292,  -9227, -12505, -15134,
        -17948, -21299, -24375, -26591, -28570, -33527, -36673, -37917],
        "X Turn Accepts (9%FP)", accept_below=-5760363)

    yturn = FixedWeightingTest([    # 70 % FP
          7513,   8450,   8958,   7553,   9312,   8054,   5367,   3690,
           440,  -1865,  -3934,  -4753,  -5483,  -6493,  -9594, -12327,
        -13942, -17793, -20918, -22503, -23521, -23443, -20509, -19126,
        -18161, -17348, -16393, -16221, -16748, -16619, -16723, -17011,
        -54125, -56415, -55035, -53946, -47605, -42570, -34993, -29244,
        -23253, -15790,  -8411,  -4454,   -911,  -1216,  -5285,  -9807,
        -12018, -13298, -18654, -25707, -26296, -23567, -18687, -20782,
        -25450, -29749, -34603, -37048, -40165, -41529, -43107, -44444,
        -55297, -51887, -48749, -45990, -39001, -33488, -28347, -24035,
        -16825,  -7860,    364,   7382,  16448,  23633,  29659,  34205,
         36730,  39299,  43290,  47077,  47705,  51974,  53213,  55577,
         57189,  59734,  62266,  62659,  61706,  64270,  65536,  65368],
         "Y Turn Accepts (70%FP)", accept_above=13924076)


    unknown = FixedWeightingTest([  # 70 % FP
         23766,  23662,  24254,  25012,  27096,  29853,  37806,  40447,
         44582,  45172,  43463,  45401,  45258,  50672,  52952,  56804,
         59877,  57231,  49602,  41161,  35540,  31689,  20571,  16995,
         16151,  16125,  10484,   6839,   6109,   5972,   3577,   3862,
         19362,  21516,  33608,  43188,  45681,  39521,  32411,  23015,
          9700,   2424,  -4169, -11686, -25987, -39646, -47287, -50814,
        -49927, -43993, -34680, -24676, -21000,  -8895,  -5398,    998,
         14490,  22525,  23709,  23345,  18327,  10732,   1906,  -6265,
        -29437, -31287, -34501, -36684, -34836, -37806, -42648, -44897,
        -52850, -54111, -50442, -52089, -56323, -64133, -65003, -65536,
        -62167, -48252, -36509, -25470, -17529,  -9033,     -7,   3084,
          3655,   1105,   4639,   1277,   3600,   6722,   9490,  12711],
          "Unknown Turn (accept between)",
          reject_below=-34544796, reject_above=-3177289)

    accept_all = SampleTest( "Pass remaining", lambda s : 1, accept_above=0 )
    return [fw16_1r, fw16_2r, xturn, yturn, unknown, accept_all]

def make_ztrigger_tests():
    fw = FixedWeightingTest([
         54012,      0,      0,   4167,  -2165,      0,      0,  -8856,
             0,      0,      0,      0,   1958,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,   1041,      0,   1789,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,  -4167,    586,
             0,   2083,      0,      0,    533,      0, -65536,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,    520,      0,
             0,  31893,      0,      0,      0,      0,      0,      0,
             0,      0,      0, -48610,      0,  54752, -60013,  -2083],
        "Fix Weight no PCA (60%)", reject_above=-446706)

    fw_fw = FixedWeightingTest([
             0,      0,      0,      0,   -512,      0,      0,      0,
             0,    256,      0,      0,      0,   2048,      0,      0,
             0,      0,      0, -42998,      0, -65536,      0,  18353,
             0,      0,  29418,      0, -16658,      0, -28211,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0, -58982,      0,      0,      0,      0,
             0,      0,      0,      0,  38698,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,  -4503,
             0, -13493,      0,  18038,      0,      0,   2882,   3602,
             0,      0,      0,      0,      0,  34828,  62634,  -4096],
        "Round 2 Fix Weight no PCA (54%)", reject_above=5230486)

    fw_yturn = FixedWeightingTest([
         26156,  23497,  19475,  15669,   9098,   5498,     17,  -4861,
         -7906,  -9712,  -9168,  -7490,  -8205,  -8986, -10649,  -9484,
         -8574,  -8183,  -6794,  -6422,  -5725,  -4599,  -3422,   -604,
          -337,   -284,    167,    328,    212,    769,    783,   1146,
        -62271, -61932, -54030, -51469, -48088, -48552, -42286, -32404,
        -29369, -18587,  -9111,   3161,  12415,  17938,  30386,  36638,
         41782,  47074,  47740,  49115,  49536,  50535,  52156,  52696,
         52346,  51421,  50717,  48475,  46952,  45137,  44108,  45227,
        -65536, -62615, -58106, -48842, -38389, -28383, -17732,  -4534,
          7378,  20030,  23182,  25973,  22934,  21008,  16182,  12977,
          8315,   4880,   2105,    721,   -628,  -1429,  -2729,  -4981,
         -6127,  -7392,  -7990,  -8102,  -7017,  -5446,  -3947,  -3245],
         "Round 3 Y-turn accepts", accept_above=-1749158)

    fw_xturn = FixedWeightingTest([
         23200,  16683,   9066,   9580,   4141,  -3962, -12810, -31655,
        -36521, -38897, -35085, -35851, -36346, -39118, -37378, -39760,
        -34636, -36256, -31652, -27804, -25864, -19212, -12286,  -7157,
         -4164,  -1562,   2518,   3528,   4049,   4963,   4682,   6877,
        -29583, -21903,   4622, -17452, -36467, -41474, -26352, -10512,
        -12657, -13850,  11788,  30983,  57374,  61410,  57328,  49723,
         54519,  65536,  60635,  60511,  62393,  62598,  61109,  60332,
         60468,  60394,  60987,  58525,  56137,  55249,  54650,  54682,
        -49395, -55834, -43578, -26345, -12388, -14322, -41967, -44087,
        -36964, -25421, -53619, -60921, -63159, -45876, -43926, -22894,
        -18644,  -9957, -12400, -13712, -14677, -16324, -17469, -19462,
        -19038, -16246, -14383, -13155, -10381,  -7032,  -4119,  -3944],
        "Round 4 X-turn accepts", accept_above=-17284718)

    fw_unknown_motion = FixedWeightingTest([
         -3955,   4483,   8721,   4478,  -2022,  -2327,   5260,   9801,
         12839,  13467,  16849,  19960,  21789,  24624,  27936,  31324,
         32231,  35895,  36346,  35233,  35162,  35665,  35846,  35786,
         35141,  34534,  33063,  32611,  31205,  30463,  29131,  28826,
        -12697,  -1905,  -2646, -13055, -23760, -36802, -46602, -54816,
        -54527, -53567, -60140, -61642, -65536, -61325, -62674, -60174,
        -60386, -56689, -46092, -39149, -35981, -34129, -31283, -29238,
        -28368, -28082, -28091, -28353, -26563, -24307, -23030, -22227,
         -8696, -19425, -22185, -17831, -11460,    792,   9893,  14622,
          9644,  12360,  13187,  15867,  10914,   6411,   8049,   6711,
          7309,   7154,   6138,   6932,   6753,   6420,   5155,   4056,
          3422,   2294,   2685,   3483,   3449,   3483,   2636,   1561],
          "Round 5, final", accept_below=0, reject_above=0)

    fw16 = FixedWeightingTest([
         24912,  23804,  24548,  21961,  22940,  17106,   8257,   4437,
         -1867,  -4358,  -1708, -10924, -16316, -24163, -23182, -17621,
        -18712, -19745, -13733, -10231,  -7649,  -4510,  -1029,   3061,
          6079,   8855,  11299,  13708,  15338,  17013,  17815,  18796,
        -20954,  -5961,  27258,  61376,  65536,  42906,   1332, -14361,
         -5567,   6993,  24603,  21632,  -7178, -27601, -25445,  -3262,
         11709,   6866,  -9518, -20687, -28206, -35551, -40934, -42713,
        -42412, -38837, -34406, -31774, -27021, -23792, -21319, -20966,
         -1257,  -4371, -16130, -20313, -16556,  -6055,  -2857, -10384,
        -18299, -20369, -27230, -16506,   5503,  25150,  27247,  15526,
          8929,   4078,   2497,    150,  -2769,  -4124,  -5289,  -5910,
         -6868,  -7921,  -8699,  -9236, -10013, -10454, -10146,  -9662],
         "Fixed Weight PCA16 (50%)", reject_below=-16887483, reject_above=-2192589)

    fw16_fw = FixedWeightingTest([
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0, -16456,      0, -65535,      0,      0,
             0,      0, -13821,  -8425,      0,      0,      0,      0,
             0,      0,      0,  -4313,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0, -13164,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,      0,      0, -50220,      0,      0,      0,
             0,      0,      0,   5392,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,      0,      0,
             0,      0,   2208,      0,      0,      0,      0,      0,
             0,      0,      0,      0,      0,      0,  20570,      0],
        "FW, run after fw16 (37%)", reject_below=-1022321, reject_above=2315226)

    fw8 = FixedWeightingTest([
         16159,  18057,  16761,  16492,  15305,  13362,  10686,   7176,
          3552,  -1998,  -4503,  -7243,  -9494, -11082, -10833, -11247,
        -11368,  -9693,  -6817,  -5634,  -5105,  -4189,  -3519,  -2784,
         -2579,  -2390,  -2364,  -1930,  -2079,  -1542,  -1177,   -854,
         -4221,  -3518,   3755,  10806,  17441,  22193,  28658,  30973,
         31740,  30832,  26556,  21020,  11277,    360, -12997, -25852,
        -35026, -42521, -48714, -51650, -55731, -58763, -61005, -62124,
        -63718, -64342, -64952, -65536, -65030, -64171, -62981, -62263,
        -34766, -39818, -41981, -42266, -40617, -36075, -30217, -21139,
        -11197,   -364,   6869,  11016,  16247,  18003,  18320,  16982,
         14171,  10739,   4395,   -541,  -4716,  -7429,  -8873,  -9701,
         -9972,  -9519,  -9164,  -8385,  -7977,  -7680,  -6624,  -5514],
        "Fixed Weight PCA8", reject_below=-28203906, reject_above=2042431)

    fw8_fw8 = FixedWeightingTest([
          2078,   1763,   -356,  -3003,  -5667,  -8669,  -9578, -12015,
        -12401, -10567,  -8263,  -5377,  -7190,  -6624,  -6394,  -8992,
        -11460, -12878, -14923, -17041, -17900, -17913, -17924, -17546,
        -16942, -16302, -14997, -14373, -13728, -13336, -12598, -12320,
         25740,  29410,  32308,  24214,  11585,  -1295, -16137, -24701,
        -22263, -18133, -11485,  -4942,   4878,  11503,  14502,  14647,
         12385,   6580,  -7587, -18281, -27832, -37093, -45541, -51581,
        -56308, -60385, -63045, -65536, -65470, -64319, -63008, -61685,
         15618,  11461,  10190,  11063,  16412,  21025,  23401,  23085,
         20951,  16994,   8413,  -1323,  -7904, -12319, -11058, -10845,
         -6881,  -2542,   1162,   4529,   7541,   9414,   9947,   9536,
          8186,   7468,   7016,   6416,   6879,   6776,   6303,   6148],
        "Fixed Weight PCA8 after first PCA8", reject_below=-16928760, reject_above=11180147)

    return [fw, fw_fw, fw_yturn, fw_xturn, fw_unknown_motion]

def make_supery_tests():
    first_half = FixedWeightingTest([    # 41% FP
         -8210,  -8060,  -8955,  -8971,  -9368,  -9104, -10555,  -9811,
        -10049, -10429, -10590, -11387, -12805, -10727, -11444, -12130,
        -12990, -13794, -17153, -16694, -15913, -16785, -17062, -17802,
        -18755, -18824, -18977, -18345, -17704, -17354, -17663, -16980,
         24225,  26623,  27654,  28450,  29231,  30294,  32617,  32675,
         34207,  33634,  34097,  34245,  34645,  36627,  37661,  37829,
         38961,  39407,  41753,  42194,  48230,  51651,  54675,  58717,
         61467,  64221,  65536,  65514,  64524,  63357,  63046,  59882,
          7585,   6706,   5865,   5378,   5563,   4845,   6663,   6186,
          6431,   5698,   4814,   4903,   6510,   3455,   3700,   2270,
          2088,   1274,   1300,    740,   1613,   1415,   1265,   1533,
          1315,   1900,   2425,   2983,   2239,   2168,   2888,   2996],
        "LD 2axis, from 16 samples", reject_below=32641288, reject_above=44881771)

    accept_all = SampleTest( "Pass remaining", lambda s : 1, accept_above=0 )
    return [first_half, accept_all]

if __name__ == "__main__":
    def parse_args():
        parser = argparse.ArgumentParser(description='Analyze an accel log dump')
        parser.add_argument('dumpfiles', nargs='+')
        parser.add_argument('-d', '--debug', action='store_true', default=False)
        parser.add_argument('-q', '--quiet', action='store_true', default=False)
        parser.add_argument('-s', '--streamed', action='store_true', default=False)
        parser.add_argument('-p', '--print-filters', action='store_true', default=False)

        parser.add_argument('-t', '--run-tests', action='store_true', default=False)
        parser.add_argument('-b', '--battery', action='store_true', default=False)
        parser.add_argument('-f', '--frequency', action='store_true', default=False)

        parser.add_argument('-w', '--export', action='store_true', default=False)
        parser.add_argument('-a', '--plot', action='store_true', default=False)
        parser.add_argument('-l', '--show-levels', action='store_true', default=False)
        return parser.parse_args()

    args = parse_args()
    if args.quiet:
        level = logging.WARNING
    elif args.debug:
        level = logging.DEBUG
    else:
        level = logging.INFO
    logging.basicConfig(level=level,
            format=' '.join(["%(levelname)-7s", "%(lineno)4d", "%(message)s"]))

    if args.streamed:
        fname = args.dumpfiles[0]
        t, x, y, z = analyze_streamed(fname, plot=args.plot)

    else:
        allsamples = Samples()
        sampleslist = []
        for fname in args.dumpfiles:
            newsamples = Samples()
            newsamples.load( fname )
            if len(newsamples.samples):
                sampleslist.append( newsamples )
                allsamples.combine( newsamples )

        if args.plot:
            allsamples.show_plots( **kwargs )
        if args.export:
            allsamples.export_csv( **kwargs )
        if args.show_levels:
            allsamples.plot_z_for_groups( only='z,xymag' )
        if args.battery:
            allsamples.plot_battery()
        if args.frequency:
            allsamples.show_wake_freq_hist()

        if False and args.run_tests:
            traditional_tests = make_traditional_tests()
            mt = MultiTest( traditional_tests,
                    allsamples.filter_samples( **kwargs ) )
            mt.run_tests(plot=args.plot)

        # find all unconfirmed samples that have full FIFO
        Zunconfirm = Samples("Unconfirmed Z Trigger")
        Yunconfirm = Samples("Unconfirmed Y Trigger")
        superYunconfirm = Samples("Unconfirmed SuperY Trigger")
        Zconfirm = Samples("Confirmed Z Trigger")
        ZYturn = Samples("Z Trigger, Y Turns")
        Yconfirm = Samples("Confirmed Y Trigger")
        superYconfirm = Samples("Confirmed Super Y Trigger")

        Zunconfirm.samples = list(allsamples.filter_samples(
            confirmed=False, full=True, triggerZ=True))
        Zconfirm.samples = list(allsamples.filter_samples(
            confirmed=True, full=True, triggerZ=True))
        Yunconfirm.samples = list(allsamples.filter_samples(
            confirmed=False, full=True, triggerY=True))
        Yconfirm.samples = list(allsamples.filter_samples(
            confirmed=True, full=True, triggerY=True))
        ZYturn.samples = list(Zconfirm.filter_samples(namehas="yturn"))
        superYunconfirm.samples = list(allsamples.filter_samples(
            confirmed=False, full=True, superY=True))
        superYconfirm.samples = list(allsamples.filter_samples(
            confirmed=True, full=True, superY=True))

        if True: # most recent sequence for starting testing
            zfilters = make_ztrigger_tests()
            zsamples = list(allsamples.filter_samples(triggerZ=True, full=True))
            mtz = MultiTest(zfilters, zsamples, name="Combined Z Trigger Tests")
            mtz.run_tests(args.plot)

            yfilters = make_ytrigger_tests()
            ysamples = list(allsamples.filter_samples(triggerY=True, full=True))
            mty = MultiTest(yfilters, ysamples, name="Combined Y Trigger Tests")
            mty.run_tests(args.plot)

            syfilters = make_supery_tests()
            sysamples = list(allsamples.filter_samples(superY=True, full=True))
            mtsy = MultiTest(syfilters, sysamples, name="Combined Super Y Trigger Tests")
            mtsy.run_tests(args.plot)

            mtz.show_result(Samples.getWakesPerDay(zsamples))
            mty.show_result(Samples.getWakesPerDay(ysamples))
            mtsy.show_result(Samples.getWakesPerDay(sysamples))

            if args.print_filters:
                print("Z ISR Filters")
                for f in zfilters:
                    f.show_xyz_filter()
                print("\nY ISR Filters")
                for f in yfilters:
                    f.show_xyz_filter()
            #unconf = list(filter_samples(fw_xturn.punted_samples, confirmed=False))
            #conf = list(filter_samples(fw_xturn.punted_samples, confirmed=True))
            #pc = PrincipalComponentTest(conf, unconf, test_axis=[0, 1, 2])
            #ld = LinearDiscriminantTest(conf, unconf, test_axis=0,
            #        prereduce=pc.getTransformationMatrix(8))
            # now we need to figure out how to set threshold
# TODO : check if we can use scipy to minimize better

        if args.run_tests:
            # add a new attribute for coloring selected data blue
            for s in Zunconfirm.samples:
                s.trainset = False
            for s in Zconfirm.samples:
                s.trainset = True

            pc_test = PrincipalComponentTest(Zunconfirm, Zconfirm,
                    test_axis=[0, 1, 2])
            pc_test.plot_result()

            #show_various_reductions(Zunconfirm, Zconfirm, wake_tests_per_day=allsamples.wake_tests_per_day):
            final_dims = 16
            pc_test.show_eigvals(final_dims)
            tf = pc_test.getTransformationMatrix(final_dims)

            ld_test = LinearDiscriminantTest(Zunconfirm, Zconfirm, test_axis=0,
                    prereduce=tf, accept_above=0, reject_below=0)
            ld_test.show_result(allsamples.wake_tests_per_day)
            ld_test.show_xyz_filter()
            #ld_test.plot_eigvals()
            ld_test.plot_weightings(0)
            ld_test.plot_result()
