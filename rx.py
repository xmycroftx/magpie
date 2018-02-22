#!/usr/bin/env python
#
# An experimental receiver for magpie's 2-FSK mode.
#
# Requires an RTL-SDR dongle and a 125MHz upconverter (eg NooElec's)
#
# Usage: python ./rx.py
#
# Very much a WIP/PoC, and needs a lot of love
#
# Copyright (c) Josh Myer <josh@joshisanerd.com>
#
# Released under the MIT license, like magpie
#


import time

import numpy as np

from gnuradio import gr
from gnuradio import filter
from gnuradio.filter import firdes
from gnuradio import blocks
from gnuradio import audio

from gnuradio import fft
from gnuradio.fft import window

import gnuradio.gr.gateway

import osmosdr

PLAY_AUDIO = False

if PLAY_AUDIO:
    Fs = 1800000
    xlate_decim = 75
else:
    Fs = 1024000
    xlate_decim = 20
    
Fc = 126246000
Foffset = 304200


Fa = Fs/xlate_decim

fft_width = 2048

symbol_rate = 5 # symbols per second

F_0 = 1200.0
F_1 = 2200.0

b_0 = int(fft_width * F_0/Fa)
b_1 = int(fft_width * F_1/Fa)

b_span = 3 # plus and minus, so 2*span+1

bit_oversample = (Fa/fft_width) / symbol_rate
print "Oversampling at %f" % bit_oversample
print "Bins at %d and %d" % (b_0, b_1)
print "Symbol rate: %d Sps" % symbol_rate

def _bit_chr(a):
    c = np.sum([a[i]*2**i for i in range(8)])
    if c > 127:
        return '~'
    if c < 0x20:
        return '_'
    return chr(c)

class Decoder(gr.gateway.decim_block):
    def __init__(self):
        gr.gateway.decim_block.__init__(self,
                                        name = "Decoder",
                                        in_sig = [(np.float32, fft_width)],
                                        out_sig = [],
                                        decim = 1)


        self.bit_acc = 0
        self.n_bits = 0

        self.bitstream = []

        self.tlast = time.time()

    def _bit(self, b):
        self.bit_acc += b
        self.n_bits += 1

        if self.n_bits == bit_oversample:
            self.bitstream.append( 1 if self.bit_acc *2 > bit_oversample else 0)
            self.bit_acc = 0
            self.n_bits = 0


            if len(self.bitstream) == 15:
                dt = time.time() - self.tlast
                self.tlast = time.time()
                print "%0.3f" % dt, "".join([_bit_chr(self.bitstream[i:]) for i in range(8)]), self.bitstream
                self.bitstream = self.bitstream[7:]
        
    def work(self, input_items, output_items):
        for a in input_items[0]:
            s_low = np.sum(a[(b_0-b_span):(b_0+b_span)])
            s_high = np.sum(a[(b_1-b_span):(b_1+b_span)])
            self._bit(1 if s_high > s_low else 0)
        return len(input_items[0])

class Rx(gr.top_block):
    def __init__(self):
        gr.top_block.__init__(self, "Rx")
        
        osmosdr_h = osmosdr.source( args="numchan=" + str(1) + " " + "" )

        osmosdr_h.set_sample_rate(Fs)
        osmosdr_h.set_center_freq(Fc, 0)
        osmosdr_h.set_gain_mode(True) # enable hw agc

        
        lpf_carrier = firdes.low_pass(1.0, Fs, 10000, 1000, firdes.WIN_HAMMING, 6.76)

        carrier_xlate = filter.freq_xlating_fir_filter_ccc(xlate_decim,
                                                           lpf_carrier,
                                                           Foffset,
                                                           Fs)

        
        am_demod = blocks.complex_to_mag(1)

        file_sink = blocks.file_sink(gr.sizeof_float*1, "test_raw", False)

        self.connect(osmosdr_h, carrier_xlate, am_demod)
        self.connect(am_demod, file_sink)
        
        if PLAY_AUDIO:
            audio_sink = audio.sink(Fa, "", True)
            self.connect(am_demod, audio_sink)


        audio_to_vec = blocks.stream_to_vector(gr.sizeof_float*1, fft_width)
        audio_fft = fft.fft_vfc(fft_width, True, (window.blackmanharris(fft_width)), 1)
        fft_mag = blocks.complex_to_mag(fft_width)
        self.connect(am_demod, audio_to_vec, audio_fft, fft_mag)
        
        decoder = Decoder()
        self.connect(fft_mag, decoder) 


        
tb = Rx()
tb.start()
tb.wait()
