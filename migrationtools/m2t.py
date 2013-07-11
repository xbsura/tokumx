#!/usr/bin/env python

# read a mongodb oplog, translate oplog entries to pymongo calls on another mongodb or tokumx database

import os
import re
import signal
import sys
import time
import traceback

import bson
from bson import son
from pymongo import MongoClient, errors

class SignalError(Exception):
    def __init__(self, sig):
        self.sig = sig
    def __str__(self):
        return 'SignalError(%d)' % self.sig

class block_signals(object):
    '''block_signals is a way to block signal processing during a critical section.

    It should be used in a with statement:

        with block_signals(signal.SIGINT, signal.SIGTERM):
            dont_interrupt_me()
    '''

    def __init__(self, *args):
        self.signals = args
        self.signaled = None
    def __enter__(self):
        print "enter"
        for s in self.signals:
            signal.signal(s, self)
        return None
    def __call__(self, sig, unused_frame):
        self.signaled = sig
    def __exit__(self, type, value, traceback):
        print "exit"
        for s in self.signals:
            signal.signal(s, signal.SIG_DFL)
        if self.signaled is not None:
            raise SignalError(self.signaled)
        return False # propogate exceptions

def main():
    ts = None
    fromhost = 'localhost:33000'
    tohost = 'localhost:55000'
    verbose = 0;
    for arg in sys.argv[1:]:
        if arg == '--verbose':
            verbose += 1
            continue
        match = re.match("--ts=(.*):(.*)", arg)
        if match:
            ts = bson.Timestamp(int(match.group(1)), int(match.group(2)))
            if verbose: print("start after", ts)
            continue
        match = re.match("--fromhost=(.*)", arg)
        if match:
            fromhost = match.group(1)
            continue
        match = re.match("--tohost=(.*)", arg)
        if match:
            tohost = match.group(1)
            continue

    # connect to fromhost and tohost
    try:
        fromv = fromhost.split(':')
        fromc = MongoClient(fromv[0], int(fromv[1]))
    except:
        print(fromv, sys.exc_info())
        return 1
    try:
        tov = tohost.split(':')
        toc = MongoClient(tov[0], int(tov[1]), w=1)
    except:
        print(tov, sys.exc_info())
        return 1

    # run a tailable cursor over the from host connection's  oplog from a point in time
    # and replay each oplog entry on the to host connection
    db = fromc.local
    oplog = db.oplog.rs
    try:
        while 1:
            if ts is None:
                qs = {}
            else:
                qs = { 'ts': { '$gt': ts }}
            if verbose: print(qs)
            c = oplog.find(qs, tailable=True, await_data=True)
            if verbose: print(c)
            if c.count() == 0:
                time.sleep(1)
            else:
                for oploge in c:
                    if verbose: print(oploge)
                    op = oploge['op']
                    with block_signals(signal.SIGHUP, signal.SIGINT, signal.SIGQUIT, signal.SIGTERM, signal.SIGCHLD):
                        last_ts = ts
                        ts = oploge['ts']
                        try:
                            replay(toc, op, oploge, verbose)
                        except errors.AutoReconnect, e:
                            print "tried to apply OpTime %d:%d but I'm not sure if it worked" % (ts.time, ts.inc)
                            ts = last_ts
                            raise
                        except errors.PyMongoError:
                            # whoops, that didn't work
                            print traceback.print_exc()
                            ts = last_ts
                            raise
    finally:
        if ts is not None:
            print('caught up to %d:%d' % (ts.time, ts.inc))
    return 0
def replay(toc, op, oploge, verbose):
    if op == 'i':
        ns = oploge['ns'].split('.',1)
        assert len(ns) == 2
        o = oploge['o']
        if verbose: print("insert", ns, o)
        db = toc[ns[0]]
        col = db[ns[1]]
        col.insert(o)
    elif op == 'd':
        ns = oploge['ns'].split('.',1)
        assert len(ns) == 2
        o = oploge['o']
        if verbose: print("delete", ns, o)
        db = toc[ns[0]]
        col = db[ns[1]]
        col.remove(o)
    elif op == 'u':
        ns = oploge['ns'].split('.',1)
        assert len(ns) == 2
        o = oploge['o']
        o2 = oploge['o2']
        if verbose: print("update", ns, o, o2)
        db = toc[ns[0]]
        col = db[ns[1]]
        col.update(o2, o)
    elif op == 'c':
        ns = oploge['ns'].split('.',1)
        assert len(ns) == 2
        assert ns[1] == '$cmd'
        o = oploge['o']
        if verbose: print("command", ns, o)
        db = toc[ns[0]]
        db.command(o)
    elif op == 'n':
        if verbose: print("nop", oploge)
    else:
        print("unknown", oploge)
        assert 0
sys.exit(main())
