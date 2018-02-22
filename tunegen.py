#!/usr/bin/python

#yeah yeah argparse me plz
import sys
filename=sys.argv[1]

def gimmebits(bs):
    bytes = (ord(bits) for bits in bs.read())
    for bits in bytes:
        for i in xrange(8):
            yield (bits >> i) & 1

def parsefile(filename):
    for bits in gimmebits(open(filename, "r")):
        if (bits == 0 ):
            print "200 1200"
        else:
            print "200 2200"

parsefile(filename)
