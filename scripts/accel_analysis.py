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
from scipy.optimize import minimize
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

    def clear_samples(self):
        self.samples = []

    def add_samples(self, samples):
        if isinstance(samples, Samples):
            self.samples.extend(samples.samples)
        else:
            self.samples.extend(samples)

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

    def plot_weightings(self):
        for tst in self.tests:
            try:
                tst.plot_weightings(show=False)
            except AttributeError:
                pass
        plt.show()

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


class AllTest(MultiTest):
    def __init__( self, *tests, **kwargs ):
        self.name = kwargs.pop('name', "Combined Test Results")
        self.tests = tests
        self.clear_results()

    def run_tests(self, plot=False):
        self.clear_results()
        for tst in self.tests:
            tst.run_tests(plot)
            self.confirmed_accepted += tst.confirmed_accepted
            self.confirmed_rejected += tst.confirmed_rejected
            self.unconfirmed_accepted += tst.unconfirmed_accepted
            self.unconfirmed_rejected += tst.unconfirmed_rejected
            self.unconfirmed_accepted_time += tst.unconfirmed_accepted_time
            self.unconfirmed_rejected_time += tst.unconfirmed_rejected_time
            if self.unconfirmed_time_cnt is None:
                self.unconfirmed_time = tst.unconfirmed_time
                self.unconfirmed_time_cnt = tst.unconfirmed_time_cnt
                self.unconfirmed_time_max = tst.unconfirmed_time_max

    def show_result(self, testsperday=None):
        for tst in self.tests:
            tst.show_result()
        super().show_result(testsperday)

    def _getConfirmedPunted(self): return 0
    confirmed_punted = property(fget=_getConfirmedPunted)

    def _getUnconfirmedPunted(self): return 0
    unconfirmed_punted = property(fget=_getUnconfirmedPunted)

    def _getUnconfirmedPuntedTime(self): return 0
    unconfirmed_punted_time = property(fget=_getUnconfirmedPuntedTime)

    def _getPuntedSamples(self): return []
    punted_samples = property(fget=_getPuntedSamples)


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
            if getattr(sample, 'red', False):
                cv = LONG
            elif getattr(sample, 'orange', False):
                cv = MED
            elif getattr(sample, 'yellow', False):
                cv = SHORT
            elif getattr(sample, 'blue', False):
                cv = TRAINSET
            elif getattr(sample, 'trainset', False):
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

    def plot_distributions(self, resolution=100, start=None, stop=None):
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

    def find_outliers(self):
        return Samples.find_outliers(self.samples, self.values)

    def plot_outliers(self, skip=None):
        Samples.plot_outliers(None, skip=skip, outliers=self.find_outliers())

    def remove_outliers(self, qty):
        return Samples.remove_outliers(self.samples, qty, self.find_outliers())

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

            print('tst_p{} = FixedWeightingTest(['.format(len(self.getWeightings())), end='\n    ')
            for j, f in enumerate(vec):
                end = ', ' if (j + 1) % 8 else ',\n    '
                if j == len(vec) - 1:
                    end = '],\n    "Test Name, p{}"'.format(len(self.getWeightings()))
                print( "{:6}".format(f), end=end )
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

    def plot_weightings(self, ndx=0, show=True):
        fig = plt.figure()
        ax = fig.add_subplot(111)
        ax.set_xlabel("Time (sample num)")
        ax.set_ylabel("Scale Factor")
        ax.set_title(self.name)
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
        if show:
            plt.show()

    def make_fixed_from_current(self, name=None):
        if name is None:
            name = "Fixed Weight Test"
        wts, scale = self.get_xyz_weights()
        fix = FixedWeightingTest(wts, name)
        if self.reject_above is not None:
            fix.reject_above = self.reject_above * scale
        if self.reject_below is not None:
            fix.reject_below = self.reject_below * scale
        if self.accept_above is not None:
            fix.accept_above = self.accept_above * scale
        if self.accept_below is not None:
            fix.accept_below = self.accept_below * scale
        return fix

    def _check_weighting_updates(self, jump=1):
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
                self.set_weightings(newwts)
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
                self.set_weightings(newwts)
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
            self.set_weightings(wts)
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
            tn, tp = self._check_weighting_updates(jp)
            updated = list(tn[1] if usenegative else tp[1])
            self.set_weightings(updated)
            self.set_thresholds()

    def minimize_false_positive(self, **kwargs):
        kwargs.setdefault('options', {'disp':True, 'xtol':128})
        kwargs.setdefault('method', 'Powell')   # or 'Nelder-Mead'
        res = minimize(self._test_updated_weights, self.getWeightings(), **kwargs)
        if res['status'] == 0:
            self.set_weightings(res['x'])
            self.set_thresholds()
        return res

    def _test_updated_weights(self, wts):
        log.info("Weights set to {}".format(wts))
        self.set_weightings(wts)
        self.set_thresholds()
        log.info("PN : {:.2%}".format(1 - self.true_negative))
        if self.reject_above == self.reject_below:
            return float('NaN')
        return (1 - self.true_negative) * 100

    def set_weightings(self, wts, **kwargs):
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
        if list(old_wts) != list(wts):
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

    def get(self, logfile, timestamp):
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self
        matches = []
        for s in sampleslist:
            if s.logfile == logfile and s.timestamp == timestamp:
                matches.append(s)
        return matches

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
        itvs = list(Samples.get_wake_intervals(self, confirmed=False))
        if len(itvs) > 0:
            return np.median(itvs)
        else:
            return 0
    wake_interval = property(fget=_getWakeIntervalMedian)

    def getWakesPerDay(self):
        day_time_hrs = 16
        day_time_sec = 3600 * day_time_hrs
        interval = Samples._getWakeIntervalMedian(self)
        if interval > 0:
            return int((day_time_sec + interval/2.0)/ interval)
        else:
            return 0
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

    def find_outliers(self, values=None):
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self
        if values is None:
            values = get_measure_matrix(sampleslist)
        mags = (int(v*10) for v in get_row_magnitudes(mean_center_columns(values)))
        return sorted(zip(mags, sampleslist))

    def plot_outliers(self, skip=None, outliers=None):
        if outliers is None:
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

    def remove_outliers(self, qty, outliers=None):
        """ in place removal of outlier samples """
        if outliers is None:
            outliers = Samples.find_outliers(self)
        outs = outliers[-qty:]
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
            quiet : no debug info
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
        quiet = kwargs.pop( 'quiet', False )

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
        if debug_filter_result and not quiet:
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

    def print_samples_summary(self):
        if isinstance(self, Samples):
            sampleslist = self.samples
        else:
            sampleslist = self

        def ffcn(*args, **kwargs):
            kwargs.setdefault('quiet', True)
            return list(Samples.filter_samples(*args, **kwargs))

        nonfull = ffcn(sampleslist, full=False)
        nonfull_confirm = ffcn(nonfull, confirmed=True)

        full = ffcn(sampleslist, full=True)

        ztrig = ffcn(full, triggerZ=True)
        ytrig = ffcn(full, triggerY=True)
        sytrig = ffcn(full, superY=True)
        zytrig = ffcn(full, triggerY=True, triggerZ=True)

        zytrig_cf = ffcn(zytrig, confirmed=True)
        zytrig_un = ffcn(zytrig, confirmed=False)

        ztrig_uncon = ffcn(ztrig, confirmed=False)
        ytrig_uncon = ffcn(ytrig, confirmed=False)
        sytrig_uncon = ffcn(sytrig, confirmed=False)

        ztrig_conf = ffcn(ztrig, confirmed=True)
        ytrig_conf = ffcn(ytrig, confirmed=True)
        sytrig_conf = ffcn(sytrig, confirmed=True)

        ztrig_xturn = ffcn(ztrig_conf, namehas='xturn')
        ztrig_yturn = ffcn(ztrig_conf, namehas='yturn')

        ytrig_xturn = ffcn(ytrig_conf, namehas='xturn')
        ytrig_yturn = ffcn(ytrig_conf, namehas='yturn')

        print("Samples Total        :{:12,}".format(len(sampleslist)))
        print("  nonfull            :{:12,}".format(len(nonfull)))
        print("  nonfull confirmed  :{:12,}".format(len(nonfull_confirm)))
        print("  full samples       :{:12,}".format(len(full)))
        print("  z&y trigger        :{:12,}".format(len(zytrig)))
        print("  z&y trigger cf     :{:12,}".format(len(zytrig_cf)))
        print("  z&y trigger un     :{:12,}".format(len(zytrig_un)))
        print("  ztrigger           :{:12,}".format(len(ztrig)))
        print("  ztrigger unconf    :{:12,}".format(len(ztrig_uncon)))
        print("  ztrigger conf      :{:12,}".format(len(ztrig_conf)))
        print("  ztrigger conf xt   :{:12,}".format(len(ztrig_xturn)))
        print("  ztrigger conf yt   :{:12,}".format(len(ztrig_yturn)))
        print("  ytrigger           :{:12,}".format(len(ytrig)))
        print("  ytrigger unconf    :{:12,}".format(len(ytrig_uncon)))
        print("  ytrigger conf      :{:12,}".format(len(ytrig_conf)))
        print("  ytrigger conf xt   :{:12,}".format(len(ytrig_xturn)))
        print("  ytrigger conf yt   :{:12,}".format(len(ytrig_yturn)))
        print("  superY             :{:12,}".format(len(sytrig)))
        print("  superY   unconf    :{:12,}".format(len(sytrig_uncon)))
        print("  superY   conf      :{:12,}".format(len(sytrig_conf)))
        print("")


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

    def __repr__(self):
        return "<{} s @ {}>".format(self.logfile, self.timestamp)

    def __hash__(self):
        return hash(self.uid)

    def __lt__(self, other):
        return self.uid < other

    def __le__(self, other):
        return self.uid <= other

    def __eq__(self, other):
        return self.uid == other

    def __ne__(self, other):
        return self.uid != other

    def __gt__(self, other):
        return self.uid > other

    def __ge__(self, other):
        return self.uid >= other

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

def make_prelim_tests( *samples ):
    nf_tst = lambda s : 1 if s.full else -1
    non_full = SampleTest("Non-full reject", nf_tst, reject_below=0)

    dt_tst = lambda s : 1 if s.triggerY and s.triggerZ else -1
    dual_trigger = SampleTest("Z&Y Dual Trigger accept", dt_tst, accept_above=0)

    return MultiTest(non_full, dual_trigger, *samples, name="Preliminary Tests")

def make_ytrigger_tests_16p(*samples):
    r1_p16 = FixedWeightingTest([       # 54 % TN
         65536,  59945,  49981,  38958,  26688,  16366,   6473,    230,
         -5822,  -7306,  -8877,  -9307,  -7720,  -7882,  -6555,  -4510,
          -233,   3643,   7762,   9908,  13120,  15241,  15561,  16374,
         16294,  15858,  15833,  14707,  12806,  11649,  11202,  10455,
         14709,  12339,   7524,   4986,   6802,  11621,  16744,  24876,
         31166,  27656,  25885,  26572,  23941,  14464,   2806, -11170,
        -24001, -33707, -39389, -38380, -32229, -21626, -13669, -10485,
         -7908,  -4670,  -1306,    704,   2430,   4438,   5461,   5261,
        -11664, -12280, -11309, -11470, -12861,  -9232, -10381, -11010,
         -9273,  -6291,  -4174,   1548,   4043,   5067,   8314,  12576,
         12228,   9428,   2192,  -6136, -13329, -18812, -22620, -23781,
        -25894, -27685, -29044, -29371, -28439, -27671, -25919, -25282],
        "Y-Trig Reject 1, p16 (54 % TN)", reject_above=-5247343)

    r2_p16 = FixedWeightingTest([       # 21 % TN
         65536,  54073,  41958,  32618,  21017,   4419, -11381, -20985,
        -25582, -30418, -36011, -42921, -50279, -53608, -59320, -58536,
        -57104, -52598, -45347, -39422, -29314, -22216,  -8095,  -1347,
          2928,   5195,   8347,  10813,  10570,  11423,  12729,  13865,
         42927,  42418,  47414,  42237,  32256,  23903,  16874,  12802,
          6580,  13367,  13610,  13019,   8570,   1822,   -516,   4184,
          9797,  14867,  22695,  29992,  41141,  44801,  52578,  60395,
         61836,  62352,  60529,  54404,  46785,  38614,  34068,  27387,
          1022,   3841,  13695,  16160,  17413,  14841,  15991,   7135,
         -3928,  -5646,  -9088,  -3604,   4154,   8448,  22753,  30834,
         38296,  42460,  43013,  41857,  34230,  24578,  15604,   5281,
           620,  -4225,  -6452,  -5672,  -5189,  -6352,  -7763, -11756],
        "Y-Trig Reject 2, p16 (21% TN)", reject_above=27381764)

    xturn_p16 = FixedWeightingTest([    # 10 % FP
        -65536, -49931, -36765, -26428, -12122,   1571,  21671,  27763,
         23998,  18673,  18306,  13502,  12936,  13198,   7648,   5476,
           478,  -7354, -15258, -22053, -26903, -31542, -35286, -37202,
        -38406, -38313, -38705, -38662, -38096, -35962, -33852, -32499,
          -467,    170,  -6102,  -7482,    -41,     45,  27560,  31146,
         16575,   4800,  11262,  14609,  14116,  17572,  12599,   6149,
          9024,   7486,   2936,  -7692, -12270, -19923, -28776, -38629,
        -44738, -51566, -54781, -52830, -51833, -47471, -41628, -35833,
         -9480, -10884, -15975, -11481,  -4722,   7388,  -2006,   1590,
         14817,  16410,  18563,  12571,   3330,   2328,  -2126,  -9722,
        -18147, -21690, -19694, -15948, -11502,  -7118,  -4166,    755,
          3431,   6848,  11294,  12712,  14389,  17784,  18502,  23190],
        "Y-trig X-turn, p16 (10% FP)", accept_above=-3369716)

    yturn_p16 = FixedWeightingTest([    # 27 % FP
          5211,   2631,  -1119,  -9511, -18960, -18771, -11579, -11364,
        -11389,  -9616,  -7828,  -2958,   2738,  11548,  13375,  15088,
         16683,  12310,  13891,   7885,   6863,   1279,  -2084,  -3565,
         -3719,  -3644,  -3803,  -3747,  -4044,  -5575,  -4952,  -4794,
         55207,  44583,  26625,   8246,   -325, -14591, -10644,   -210,
          6546,   1876,   1322,   4647,  16712,  23295,  20993,  24372,
         15377,   7473,   1213,   7445,   6336,   6661,   3665,   4672,
          8676,  11778,  16456,  19993,  21630,  22619,  18637,  12525,
         65536,  60398,  55516,  44893,  24506,  19844,  -8077, -22209,
        -22959, -24857, -22200, -17291, -22797, -30112, -33944, -34146,
        -33979, -30378, -21656, -13682,     38,  10879,  16830,  14807,
         13005,   6921,  -1600, -10751, -20074, -24517, -29758, -31890],
        "Y-trig Y-turn, p16 (27% FP)", accept_below=-5038357)

    last_p16 = FixedWeightingTest([     # 21% FN / 32% FP
         48184,  35667,  23211,   8229,  -5223, -12821, -22689, -29053,
        -29821, -27708, -28852, -30206, -24935,  -6844,  -5136,  11271,
         24249,  28373,  37889,  31606,  32962,  32362,  29479,  29664,
         29963,  29727,  28481,  26709,  25063,  22635,  23585,  24750,
         18315,  18483,  15409,  11828,   8509,  -1848, -11546, -12459,
         -4814,  -7265, -11364,  -4908,   6676,  21010,  17902,  13318,
         -9071, -26985, -48336, -49047, -35878, -20732, -11187,   1281,
         12155,  20178,  26058,  29768,  24026,  23977,  13662,   -434,
         65536,  58896,  57443,  50925,  27300,   9456,  -5632, -19973,
        -27598, -29786, -23486,  -5898, -15949, -29988, -31078, -26428,
        -23837, -13313,  -5042,  -5234,   2425,   6246,  11803,   7638,
          6912,  -1915,  -8072, -14550, -19565, -17125, -19017, -20902],
        "Y-Trig Last Test, p16 (32% FP / 21% FN)", reject_above=0, accept_below=0)

    return MultiTest(r1_p16, r2_p16, xturn_p16, yturn_p16, last_p16,
            *samples, name="Y-Trigger, 16 parameters")

def make_ytrigger_tests_8p(*samples):
    r1_p8 = FixedWeightingTest([        # 45 % TN
        -28247, -26720, -25102, -23099, -20643, -19454, -16719, -16150,
        -16157, -16853, -16253, -15324, -14883, -13047, -12133, -10941,
        -10783, -10014,  -9170,  -8288,  -7870,  -7449,  -6238,  -5228,
         -4938,  -4839,  -5351,  -4902,  -4428,  -3490,  -3509,  -3143,
         18514,  18540,  18072,  15983,  12543,   6023,  -1134,  -9280,
        -14424, -15368, -17980, -18404, -20052, -15961,  -8334,    451,
          6143,   9584,  10472,   9282,   8543,   5502,   2296,   1415,
          -451,  -2813,  -5286,  -7808, -10275, -13854, -16315, -16679,
           572,    425,   1331,   -877,  -1846,  -6950,  -9807, -13554,
        -17902, -19724, -20019, -20088, -19714, -17269, -14184, -10445,
         -3591,   4126,  13228,  23469,  31653,  38310,  45687,  49847,
         54996,  59684,  64015,  65536,  65057,  63945,  62580,  59360],
        "PCA8, Reject Test 1 (45%)", reject_below=2733591, reject_above=34764674)

    r2_p8 = FixedWeightingTest([        # 22 % TN
         57961,  61043,  63047,  63874,  64606,  65535,  65195,  64894,
         61653,  59486,  57407,  58174,  55736,  53252,  49559,  45092,
         42341,  37142,  33296,  30578,  27535,  24511,  17972,  15209,
         13356,  10583,   8208,   5450,   3449,   3012,   2127,   2375,
         42129,  45340,  44132,  43850,  48841,  47395,  50848,  51593,
         53497,  52777,  47470,  41659,  30621,  17853,   2120, -14609,
        -25616, -29882, -27270, -25170, -25434, -24403, -25020, -29960,
        -33557, -36306, -35631, -32567, -30214, -25485, -24595, -23653,
         59988,  61252,  61520,  60255,  58120,  54404,  48918,  42721,
         39579,  35189,  29771,  25172,  21581,  16288,   9839,   4090,
          1298,   -272,    641,   1223,   2904,   5322,   6212,   8401,
          8260,   9118,   9555,  10099,  11304,  11484,  10890,  11501],
        "PCA8, Reject Test 2 (22%)", reject_below=-89832048, reject_above=29154581)

    xturn_p8 = FixedWeightingTest([     # 16 % FP
         65536,  53201,  43038,  34966,  25535,  17619,   9357,   1228,
         -4127,  -8943, -11623, -15508, -18465, -20734, -21103, -19894,
        -20036, -15790, -11932, -11583,  -8788,  -8774,  -2441,  -1155,
           -12,   1443,   3094,   4335,   4473,   2330,   2197,   2702,
         15181,   8550,   3726,  -2067,  -6330,  -9336, -11567, -12808,
        -16538, -18098, -16700, -19178, -19090, -15092, -10560,    368,
          7159,  11027,  11345,  13255,  20641,  29159,  38235,  48710,
         54367,  59128,  61376,  58651,  57409,  52008,  47989,  41592,
         15238,  13208,  11740,   8631,   7531,  10410,   9895,   8488,
          5775,   2729,    -18,   -917,  -3356,  -5132,  -7232,  -9073,
         -6991,  -6219,  -3191,  -4992,  -8170,  -9965,  -9885, -12373,
        -12158, -11830, -11633, -12837, -14729, -16347, -16449, -20403],
        "Y-trig X-turn Accept (PCA8, FP 16%)", accept_below=3269748)

    yturn_p8 = FixedWeightingTest([     # 14 % FP
         -6274,  -6995,  -6853,  -5462,  -3768,  -3804,  -1949,   -627,
          2644,   4716,   6570,   6000,   5023,   4181,   4146,   7215,
          7001,   6894,   5917,   3448,   3488,   3478,   1460,     24,
          -700,   -534,  -1482,  -1274,  -1889,  -1462,  -1165,    498,
         47530,  45827,  47329,  46543,  47500,  39233,  36210,  30622,
          7527,  -1956,  -3861,  -6565,  -8850,  -2663,  17811,  30337,
         32955,  35625,  36183,  34297,  31111,  22736,  11936,  11390,
         11127,   8611,   7075,   5431,   4133,   1025,   -230,  -3173,
         65535,  60579,  57180,  50562,  42620,  33835,  27760,  17647,
          -441, -17623, -30836, -44245, -52466, -57851, -58433, -57739,
        -49391, -43390, -33711, -25982, -20303, -14384,  -2741,  -1191,
          4131,   8108,   9231,   8688,   7429,   7087,   3718,   1772],
        "Y-trig Y-turn Accept (PCA8, FP 14%)", accept_below=-13211549)

    last_p8 = FixedWeightingTest([
           875,   1455,   2967,   3329,   4117,   4681,   4213,   5223,
         11094,   9426,  10778,  11242,   8419,   6770,   4561,    181,
         -3241,  -2474,  -1254,   -589,     58,  -1959,  -1772,   -898,
         -1634,  -2112,  -3274,  -4059,  -6223,  -4204,  -5376,  -4537,
         65536,  61232,  56366,  51409,  51652,  46910,  42081,  44920,
         32305,  33810,  31785,  27682,  13671,  -2106,  -4544, -20085,
        -32984, -34875, -24709, -20044, -17095, -15706, -14056, -16639,
        -21309, -24412, -27448, -26347, -27058, -22404, -26025, -26796,
         49577,  47392,  44007,  38492,  30898,  26335,  27750,  19467,
          8738,  -5727, -15988, -23553, -24545, -27792, -31910, -36103,
        -34737, -30514, -25906, -27430, -25064, -22688, -16886, -15474,
        -13882, -11034, -10079, -10972,  -9973, -11085, -18889, -20353],
        "Y-Trig, last ditch", reject_above=-12426520, accept_below=-12426520)

    return MultiTest(r1_p8, r2_p8, xturn_p8, yturn_p8, last_p8,
            *samples, name="Y-Trigger 8 parameters")

def make_ztrigger_tests_16p( *samples ):
    r1_p16 = FixedWeightingTest([      # 56 % TN
        -51347, -48077, -42486, -37898, -31478, -23230, -15681,  -3044,
          5742,  13002,  19745,  18857,  20364,  20381,  18719,  22160,
         21903,  18372,  12641,   6686,   1465,  -4473, -10102, -15362,
        -20652, -25133, -29800, -33410, -35683, -37372, -37717, -39009,
        -28252, -31226, -41494, -47809, -47692, -45804, -36032, -15669,
          1311,  15615,  19757,  11384, -10580, -27742, -31937, -14081,
         11544,  31215,  44965,  57203,  64124,  65536,  63401,  57723,
         50249,  42671,  35847,  30392,  25343,  21758,  19832,  18864,
        -30502, -27274, -18393,  -9371,  -1320,   5333,  10559,   6634,
          2268,  -2008,  -1924,   7310,  17254,  24887,  26088,  17219,
          8011,  -3619, -10502, -18100, -25004, -30384, -34333, -36185,
        -36630, -36124, -35776, -34678, -33799, -32929, -31516, -29008],
        "Z-Trig Reject 1, PCA16 (56% TN)", reject_below=-13190000)

    r2_p16 = FixedWeightingTest([      # 50% TN
         -1598,  -3149,  -3331,  -5139, -10611, -13203, -20036, -16331,
        -16261, -12024,  -7724,  -6382,  -6183,  -6885,  -6828,  -4740,
         -1662,    882,   4452,   6473,   7581,   9609,  10491,  11600,
         12215,  12287,  12204,  11891,  11718,  11665,  11248,  10436,
           954,  12154,  33370,  40979,  34784,  16379,  -1838,  -7073,
         -4617,  -1576,   -867,  -2646,    693,   3222,   8781,   8128,
         -2346, -10404, -11391, -12050,  -7619,     83,  10648,  22286,
         33520,  44090,  52170,  58972,  63651,  65536,  65349,  64421,
        -21432, -26609, -28047, -24722, -15949,  -5230,    432,  -4118,
        -15223, -24718, -35964, -37656, -35458, -29972, -17863,  -7150,
           -90,   2099,    970,    143,    224,    870,   2232,   3768,
          5499,   6255,   7414,   7894,   8217,   8722,   8481,   8286],
        "Z-Trig Reject 2, PCA16 (50% TN)", reject_below=-10335557)

    xturn_p16 = FixedWeightingTest([   # 15 % FP
         51176,  45168,  39144,  32818,  28271,  23880,  20259,  12040,
          6741,   -605,  -3531,  -6916,  -7938, -11686, -10266,  -8286,
         -8186,  -7968,  -9355, -10954, -12092, -13478, -14802, -15699,
        -18000, -18416, -17767, -18998, -19628, -19770, -19717, -20086,
        -45600, -46858, -43828, -45274, -46667, -41052, -36832, -28345,
        -24839, -23567, -20213, -21185, -26837, -35807, -39165, -11357,
         12820,  18328,  10963,   7896,   7490,  -1187, -12759, -21423,
        -29688, -34584, -40994, -44713, -51083, -53259, -53223, -54849,
         35115,  35155,  36170,  38475,  37335,  39242,  34888,  34622,
         39350,  43521,  47446,  48619,  36543,  35630,  39072,  30156,
         24509,  26526,  38178,  48813,  56435,  62373,  65124,  65277,
         65536,  64111,  61968,  61486,  60456,  55759,  50675,  44920],
        "Z-Trig, X-turn, PCA16 (15% FP)", accept_below=37133677)

    yturn_p16 = FixedWeightingTest([   # 15 % FP
         16755,  14842,  14203,  13008,  11956,  10938,  12033,   5760,
          1135,  -4827,  -3133,  -4496,  -5570,  -7453,  -8216,  -4032,
          -281,   -624,  -2540,  -1467,    318,    385,    247,    151,
           365,   1208,   1689,   1244,   1353,   2179,   2290,   2744,
         38595,  32554,  25867,  17554,   9220,   7777,   9929,   1932,
          -675,    195,   6060,   7200,    622,   3282,   3572,  -6035,
        -24034, -32394, -24541, -16970, -12538, -11811, -14261, -18831,
        -24958, -32540, -42059, -48211, -55374, -59913, -63297, -65536,
         45949,  43056,  36298,  28865,  18826,   9539,   1766,   1486,
         -1335,  -4755,  -6034,  -4338,  -3086,   -656,   5622,   5798,
          9042,  12639,  14681,  18853,  23575,  28671,  31920,  33707,
         34188,  33900,  33383,  34371,  35287,  34015,  31584,  27731],
        "Z-Trig, Y-Turn, PCA16 (15% FP)", accept_below=11316960)

    last_p16 = FixedWeightingTest([     # 15 % FN / 50 % FP
          7327,   6693,   9767,   4830,   3493,   7682,  -9335,  -6702,
         -6951,  -7546,   1775,  -1228,   7107,  -4643, -36264,   4760,
         25153,  13091,   9983,  11647,   9898,   9866,   4989,   1800,
         -2632,  -8827, -14579, -18911, -22962, -26465, -29441, -30635,
          5437,  14543,  21267,  19032,  15217, -17816, -52705, -29276,
          8263,  36770,  40468,  16185, -29008, -59610,  -7393,  18941,
        -12467, -37108, -10088,   1630,   3515,   -945,  -5591, -11920,
        -24650, -30463, -37432, -39648, -42092, -44266, -46665, -47718,
         26507,  26026,  19551,   8388,  14708,  13373,  19866,   6804,
         -1933, -16180, -33835, -15835, -26415,  29419,  65535,  30407,
         17313,  15092,   9038,   4427,   2283,   3655,   6843,   8118,
          9098,   9990,  12151,  13967,  15845,  14884,  12870,   9248],
        "Z-Trig, Last Test PC16 (15%FN / 50%FP)", reject_above=9848706, accept_below=9848706)

    return MultiTest(r1_p16, r2_p16, xturn_p16, yturn_p16, last_p16,
            *samples, name="Z-Trigger, 16 parameters")

def make_ztrigger_tests_8p( *samples ):
    r1_p8 = FixedWeightingTest([        # 60% TN
        -46069, -46144, -44355, -42451, -39345, -35364, -31387, -26652,
        -22643, -16698, -12573,  -8403,  -5306,  -2786,  -1736,   -865,
           355,    158,  -1419,  -1862,  -2342,  -2641,  -3011,  -3209,
         -3370,  -3748,  -4275,  -4687,  -4784,  -5040,  -5317,  -5757,
        -56850, -57900, -59085, -59879, -58936, -55982, -53054, -47855,
        -42200, -32979, -22206, -10457,   4305,  19328,  34820,  50053,
         60805,  65536,  61487,  58113,  56716,  56409,  56421,  56642,
         57394,  57919,  57990,  58371,  57809,  57526,  56714,  56640,
        -16977, -13670, -11468,  -9972,  -9848, -11131, -12904, -15495,
        -19163, -24006, -27212, -29270, -30104, -30196, -29660, -27650,
        -24477, -20958, -16020, -13445, -11745, -11079, -11409, -11385,
        -11600, -11461, -11550, -11455, -11080, -10770, -10355,  -9891],
        "Z-Trig Reject 1, p8 (60% TN)", reject_below=-16000000)

    r2_p8 = FixedWeightingTest([        # 54% TN
         12486,  13725,  14085,  15107,  15356,  15490,  16282,  14347,
         14247,  12252,  12604,  13604,  10409,  10006,   7512,   3544,
           -85,  -3804,  -7131,  -9546, -11228, -12737, -13555, -14296,
        -14260, -14127, -14020, -13555, -13209, -13065, -12956, -12990,
         -8998,  -9303,  -5935,  -2693,    836,   6087,  10736,  14661,
         20598,  24770,  27022,  29432,  30365,  25855,  18506,  10537,
           163, -11469, -24194, -32858, -40403, -47526, -53569, -57186,
        -59996, -62309, -64313, -65536, -65371, -64766, -63686, -63204,
         29964,  28524,  27004,  26658,  25746,  24640,  22947,  21631,
         22055,  19366,  15944,  11129,   8090,   4188,   2156,     81,
         -1059,  -2186,  -3006,  -2843,  -2258,  -1390,  -1101,  -1504,
         -2029,  -2398,  -2603,  -2873,  -2420,  -2174,  -1758,   -902],
        "Z-Trig Reject 2, p8", reject_above=6300000)

    xturn_p8 = FixedWeightingTest([     # 16% FP
        -62003, -54521, -46792, -40449, -33693, -25897, -18499, -10176,
         -3628,   4441,  15188,  25097,  31121,  34458,  32442,  31097,
         30737,  28095,  21955,  17540,  13870,  11065,   8179,   4477,
          1888,   -942,  -3723,  -6504,  -8513,  -9857, -10715, -12289,
         17411,  21501,  31263,  40458,  46468,  52997,  62101,  58930,
         60805,  61856,  65536,  60954,  54758,  36016,  19658,   5858,
          -591,  -3541,  -6785,  -8425, -12791, -15777, -16292, -15047,
        -13772, -14075, -14268, -13334,  -9994,  -9294,  -8086, -10283,
        -30659, -34025, -34592, -33630, -33556, -31745, -27600, -21390,
        -16187, -13724, -12809, -16434, -16456, -20104, -22495, -21833,
        -20663, -21108, -24190, -28431, -31250, -33881, -35538, -35810,
        -34336, -32468, -29920, -27872, -25250, -22389, -19285, -14970],
        "Z-Trig X-Turn, p8 (16% FP)", accept_above=-21500000)

    yturn_p8 = FixedWeightingTest([     # 13% FP
          1619,   1914,   3066,   3228,   3365,   4161,   5589,   5329,
          5969,   4275,   4522,   8261,   6526,   6760,   8495,   7035,
          4194,   2443,   -771,  -2023,  -1966,  -2063,  -1968,  -2335,
         -2066,  -1514,  -1501,  -1233,  -1401,  -1695,  -2193,  -2418,
         50225,  50640,  51778,  50564,  45519,  39643,  36879,  28327,
         23054,  14245,   3808,  -2204,  -4610,  -8389, -13528, -20095,
        -23002, -24628, -28397, -31305, -35208, -40485, -46564, -52876,
        -56593, -59116, -61717, -63926, -64311, -64596, -64757, -65536,
         40778,  37076,  33595,  29920,  25387,  20883,  13891,   8537,
          5098,   -129,  -2739,  -9843, -12195, -14121, -13836, -12759,
         -9199,  -4040,    318,   3560,   6906,   8993,   9929,   9781,
          9774,   9935,   9415,   9479,   9627,   9618,   8618,   8150],
        "Z-Trig Y-turn, p8, (13% FP)", accept_below=-7000000)

    last_p8 = FixedWeightingTest([      # 19% FN / 50% FP
        -12455,  -7569,  -4377,    493,   2273,   5681,  11216,  13963,
         14845,  16116,  20008,  23619,  22365,  23892,  18620,  17352,
         14123,  12447,   9129,   4166,  -1196,  -5016,  -8362, -13317,
        -17691, -21072, -24463, -27338, -29645, -29198, -29463, -28971,
        -60242, -58163, -49431, -39155, -27671, -13909,  -3629,   3618,
         13108,  26913,  41507,  53107,  65536,  60063,  38553,  17326,
          6301,  -1401, -12112, -18491, -22839, -20322, -16623,  -9123,
         -2016,   3283,   6237,   9264,  14111,  14898,  15569,  15244,
        -30187, -28678, -26118, -21360, -17288, -11746,  -4803,   6812,
         16505,  21095,  22835,  20888,  25168,  18693,  15137,  12616,
          6273,  -5469, -20457, -33717, -43492, -49568, -54581, -53827,
        -51180, -48206, -44331, -41312, -36509, -30967, -26966, -21778],
        "Z-Trig Last, p8 (19% FN / 50% FP)",
        reject_below=-17000000, accept_above=-17000000)

    return MultiTest(r1_p8, r2_p8, xturn_p8, yturn_p8, last_p8,
            *samples, name="Z-Trigger 8 parameters")

def make_ztrigger_tests_full(*samples):
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

    return MultiTest(fw, fw_fw, fw_yturn, fw_xturn, fw_unknown_motion,
            *samples, name="Combined Z-Trigger")

def make_ztrigger_tests(*samples): return make_ztrigger_tests_8p(*samples)
def make_ytrigger_tests(*samples): return make_ytrigger_tests_8p(*samples)

def make_supery_tests( *samples ):

    r1_p2 = FixedWeightingTest([        # 71 % TN
        -11652, -11398, -12497, -12737, -13268, -13077, -14563, -14039,
        -14372, -14908, -15189, -16015, -17854, -15700, -16329, -16745,
        -17812, -18649, -21944, -21446, -20699, -21524, -21625, -22349,
        -23322, -23412, -23616, -23062, -22519, -22069, -22506, -21782,
         22172,  25031,  26343,  27079,  27709,  29109,  31328,  31413,
         32988,  32616,  33130,  33822,  33892,  36217,  37618,  38224,
         39414,  39939,  42493,  43275,  49178,  52514,  55402,  58964,
         61736,  64244,  65358,  65536,  64441,  63419,  63298,  60449,
         10632,  10160,   9421,   9137,   9144,   8378,   9913,   9381,
          9313,   8369,   7101,   7070,   8643,   5706,   5942,   4586,
          4535,   3726,   3714,   3020,   3373,   3090,   2784,   2824,
          2561,   2906,   3279,   3660,   3054,   2822,   3390,   3290],
        "SuperY Reject 1, p2 (71% TN)", reject_below=34846570, reject_above=44016013)

    accept_all = SampleTest( "Pass remaining", lambda s : 1, accept_above=0 )

    return MultiTest( r1_p2, accept_all, *samples, name="SuperY Trigger, 2 param" )


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

        if True: # most recent sequence for starting testing
            ptest = make_prelim_tests(allsamples)
            ptest.run_tests()

            tst_samples = ptest.punted_samples

            # Create sample sets for all remaining samples
            Zsamples = Samples("Z-Trigger")
            Ysamples = Samples("Y-Trigger")
            sYsamples = Samples("Super Y-Trigger")
            Zunconfirm = Samples("Unconfirmed Z Trigger")
            Yunconfirm = Samples("Unconfirmed Y Trigger")
            sYunconfirm = Samples("Unconfirmed SuperY Trigger")
            Zconfirm = Samples("Confirmed Z Trigger")
            Yconfirm = Samples("Confirmed Y Trigger")
            sYconfirm = Samples("Confirmed Super Y Trigger")

            Zsamples.samples = list(Samples.filter_samples(tst_samples, triggerZ=True))
            Ysamples.samples = list(Samples.filter_samples(tst_samples, triggerY=True))
            sYsamples.samples = list(Samples.filter_samples(tst_samples, superY=True))
            if len(list(Samples.filter_samples(tst_samples,
                triggerZ=False, triggerY=False, superY=False))):
                raise Exception("Somehow we missed testing a sample!")

            ytest = make_ytrigger_tests(Ysamples)
            ztest = make_ztrigger_tests(Zsamples)
            sytest = make_supery_tests(sYsamples)

            alltests = AllTest(ptest, ztest, ytest, sytest)
            alltests.run_tests()

            Zunconfirm.samples = list(Zsamples.filter_samples(confirmed=False))
            Zconfirm.samples = list(Zsamples.filter_samples(confirmed=True))
            if 'swi_2016-03-08.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(1)
            if 'swi_2016-03-15_yturn.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(3)
            if 'swi_2016-03-15b_yturn.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(3)
            if 'pf_2016-03-13.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(1)
            if 'rab_2016-03-15.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(1)
            if 'rab_2016-03-15b.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(1)
            if 'rab_2016-03-16a.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(3)
            if 'rab_2016-03-11.log' in Zconfirm.get_file_names():
                Zconfirm.remove_outliers(2)
            Yunconfirm.samples = list(Ysamples.filter_samples(confirmed=False))
            Yconfirm.samples = list(Ysamples.filter_samples(confirmed=True))
            if 'rab_2016-03-09.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(1)
            if 'rab_2016-03-10.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(3)
            if 'rab_2016-03-11b.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(1)
            if 'pf_2016-03-13.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(1)
            if 'rab_2016-03-14.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(1)
            if 'swi_2016-03-15b_yturn.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(1)
            if 'rab_2016-03-15.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(1)
            if 'rab_2016-03-15b.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(3)
            if 'rab_2016-03-16a.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(3)
            if 'swi_2016-03-17b_xturn.log' in Yconfirm.get_file_names():
                Yconfirm.remove_outliers(1)
            sYunconfirm.samples = list(sYsamples.filter_samples(confirmed=False))
            sYconfirm.samples = list(sYsamples.filter_samples(confirmed=True))

            zc = [s for s in Samples.filter_samples(ztest.punted_samples, confirmed=True) if s in Zconfirm.samples]
            zcy = list(Samples.filter_samples(zc, namehas='yturn'))
            if 'rab_2016-03-15c_yturn.log' in Samples.get_file_names(zcy):
                Samples.remove_outliers(zcy, 1)
            zcx = list(Samples.filter_samples(zc, namehas='xturn'))
            if 'rab_2016-03-15d_xturn.log' in Samples.get_file_names(zcx):
                Samples.remove_outliers(zcx, 1)
            zu = list(Samples.filter_samples(ztest.punted_samples, confirmed=False))

            yc = [s for s in Samples.filter_samples(ytest.punted_samples, confirmed=True) if s in Yconfirm.samples]
            ycy = list(Samples.filter_samples(yc, namehas='yturn'))
            ycx = list(Samples.filter_samples(yc, namehas='xturn'))
            if 'rab_2016-03-15d_xturn.log' in Samples.get_file_names(ycx):
                Samples.remove_outliers(ycx, 2)
            if 'swi_2016-03-17b_xturn.log' in Samples.get_file_names(ycx):
                Samples.remove_outliers(ycx, 1)
            if 'swi_2016-03-17d_xturn.log' in Samples.get_file_names(ycx):
                Samples.remove_outliers(ycx, 1)
            if 'swi_2016-03-17e_xturn.log' in Samples.get_file_names(ycx):
                Samples.remove_outliers(ycx, 1)
            yu = list(Samples.filter_samples(ytest.punted_samples, confirmed=False))

            allsamples.print_samples_summary()

            alltests.show_result(allsamples.wake_tests_per_day)

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
                s.blue = False
            for s in Zconfirm.samples:
                s.blue = True

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
