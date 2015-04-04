#!/usr/bin/env python
##################################################
# Gnuradio Python Flow Graph
# Title: Top Block
# Generated: Thu Mar 20 09:16:32 2014
##################################################

from gnuradio import analog
from gnuradio import blocks
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import wxgui
from gnuradio.eng_option import eng_option
from gnuradio.fft import window
from gnuradio.filter import firdes
from gnuradio.wxgui import fftsink2
from gnuradio.wxgui import forms
from grc_gnuradio import wxgui as grc_wxgui
from optparse import OptionParser
import osmosdr
import wx

class top_block(grc_wxgui.top_block_gui):

    def __init__(self):
        grc_wxgui.top_block_gui.__init__(self, title="Top Block")

        ##################################################
        # Variables
        ##################################################
        self.tuning = tuning = 450000000
        self.squelch = squelch = -50
        self.samp_rate = samp_rate = 20000000
        self.q_offset = q_offset = -0.005
        self.phase = phase = -0.045
        self.magnitude = magnitude = 0.032
        self.i_offset = i_offset = 0.002

        ##################################################
        # Blocks
        ##################################################
        _q_offset_sizer = wx.BoxSizer(wx.VERTICAL)
        self._q_offset_text_box = forms.text_box(
        	parent=self.GetWin(),
        	sizer=_q_offset_sizer,
        	value=self.q_offset,
        	callback=self.set_q_offset,
        	label="DC offset Q",
        	converter=forms.float_converter(),
        	proportion=0,
        )
        self._q_offset_slider = forms.slider(
        	parent=self.GetWin(),
        	sizer=_q_offset_sizer,
        	value=self.q_offset,
        	callback=self.set_q_offset,
        	minimum=-0.1,
        	maximum=0.1,
        	num_steps=200,
        	style=wx.SL_HORIZONTAL,
        	cast=float,
        	proportion=1,
        )
        self.GridAdd(_q_offset_sizer, 3, 0, 1, 7)
        _magnitude_sizer = wx.BoxSizer(wx.VERTICAL)
        self._magnitude_text_box = forms.text_box(
        	parent=self.GetWin(),
        	sizer=_magnitude_sizer,
        	value=self.magnitude,
        	callback=self.set_magnitude,
        	label="Magnitude correction",
        	converter=forms.float_converter(),
        	proportion=0,
        )
        self._magnitude_slider = forms.slider(
        	parent=self.GetWin(),
        	sizer=_magnitude_sizer,
        	value=self.magnitude,
        	callback=self.set_magnitude,
        	minimum=-0.1,
        	maximum=0.1,
        	num_steps=200,
        	style=wx.SL_HORIZONTAL,
        	cast=float,
        	proportion=1,
        )
        self.GridAdd(_magnitude_sizer, 1, 0, 1, 7)
        _i_offset_sizer = wx.BoxSizer(wx.VERTICAL)
        self._i_offset_text_box = forms.text_box(
        	parent=self.GetWin(),
        	sizer=_i_offset_sizer,
        	value=self.i_offset,
        	callback=self.set_i_offset,
        	label="DC offset I",
        	converter=forms.float_converter(),
        	proportion=0,
        )
        self._i_offset_slider = forms.slider(
        	parent=self.GetWin(),
        	sizer=_i_offset_sizer,
        	value=self.i_offset,
        	callback=self.set_i_offset,
        	minimum=-0.1,
        	maximum=0.1,
        	num_steps=200,
        	style=wx.SL_HORIZONTAL,
        	cast=float,
        	proportion=1,
        )
        self.GridAdd(_i_offset_sizer, 2, 0, 1, 7)
        self.wxgui_fftsink2_0 = fftsink2.fft_sink_c(
        	self.GetWin(),
        	baseband_freq=0,
        	y_per_div=10,
        	y_divs=10,
        	ref_level=0,
        	ref_scale=2.0,
        	sample_rate=samp_rate,
        	fft_size=1024,
        	fft_rate=15,
        	average=False,
        	avg_alpha=None,
        	title="FFT Plot",
        	peak_hold=False,
        )
        self.Add(self.wxgui_fftsink2_0.win)
        _tuning_sizer = wx.BoxSizer(wx.VERTICAL)
        self._tuning_text_box = forms.text_box(
        	parent=self.GetWin(),
        	sizer=_tuning_sizer,
        	value=self.tuning,
        	callback=self.set_tuning,
        	label="LO Tuning",
        	converter=forms.int_converter(),
        	proportion=0,
        )
        self._tuning_slider = forms.slider(
        	parent=self.GetWin(),
        	sizer=_tuning_sizer,
        	value=self.tuning,
        	callback=self.set_tuning,
        	minimum=300000000,
        	maximum=3800000000,
        	num_steps=200,
        	style=wx.SL_HORIZONTAL,
        	cast=int,
        	proportion=1,
        )
        self.GridAdd(_tuning_sizer, 4, 0, 1, 7)
        _squelch_sizer = wx.BoxSizer(wx.VERTICAL)
        self._squelch_text_box = forms.text_box(
        	parent=self.GetWin(),
        	sizer=_squelch_sizer,
        	value=self.squelch,
        	callback=self.set_squelch,
        	label="squelch",
        	converter=forms.float_converter(),
        	proportion=0,
        )
        self._squelch_slider = forms.slider(
        	parent=self.GetWin(),
        	sizer=_squelch_sizer,
        	value=self.squelch,
        	callback=self.set_squelch,
        	minimum=-150,
        	maximum=-50,
        	num_steps=100,
        	style=wx.SL_HORIZONTAL,
        	cast=float,
        	proportion=1,
        )
        self.Add(_squelch_sizer)
        _phase_sizer = wx.BoxSizer(wx.VERTICAL)
        self._phase_text_box = forms.text_box(
        	parent=self.GetWin(),
        	sizer=_phase_sizer,
        	value=self.phase,
        	callback=self.set_phase,
        	label="Phase correction",
        	converter=forms.float_converter(),
        	proportion=0,
        )
        self._phase_slider = forms.slider(
        	parent=self.GetWin(),
        	sizer=_phase_sizer,
        	value=self.phase,
        	callback=self.set_phase,
        	minimum=-0.1,
        	maximum=0.1,
        	num_steps=200,
        	style=wx.SL_HORIZONTAL,
        	cast=float,
        	proportion=1,
        )
        self.GridAdd(_phase_sizer, 0, 0, 1, 7)
        self.osmosdr_sink_0 = osmosdr.sink( args="numchan=" + str(1) + " " + "" )
        self.osmosdr_sink_0.set_sample_rate(samp_rate)
        self.osmosdr_sink_0.set_center_freq(441000000, 0)
        self.osmosdr_sink_0.set_freq_corr(0, 0)
        self.osmosdr_sink_0.set_gain(10, 0)
        self.osmosdr_sink_0.set_if_gain(20, 0)
        self.osmosdr_sink_0.set_bb_gain(20, 0)
        self.osmosdr_sink_0.set_antenna("", 0)
        self.osmosdr_sink_0.set_bandwidth(0, 0)
          
        self.blocks_multiply_const_vxx_0 = blocks.multiply_const_vcc(((1 + magnitude) + (1j - magnitude), ))
        self.blocks_add_const_vxx_1 = blocks.add_const_vcc((i_offset + 1j * q_offset, ))
        self.analog_sig_source_x_0 = analog.sig_source_c(samp_rate, analog.GR_SIN_WAVE, 100000, 1, 0)

        ##################################################
        # Connections
        ##################################################
        self.connect((self.analog_sig_source_x_0, 0), (self.wxgui_fftsink2_0, 0))
        self.connect((self.blocks_add_const_vxx_1, 0), (self.osmosdr_sink_0, 0))
        self.connect((self.analog_sig_source_x_0, 0), (self.blocks_multiply_const_vxx_0, 0))
        self.connect((self.blocks_multiply_const_vxx_0, 0), (self.blocks_add_const_vxx_1, 0))


# QT sink close method reimplementation

    def get_tuning(self):
        return self.tuning

    def set_tuning(self, tuning):
        self.tuning = tuning
        self._tuning_slider.set_value(self.tuning)
        self._tuning_text_box.set_value(self.tuning)

    def get_squelch(self):
        return self.squelch

    def set_squelch(self, squelch):
        self.squelch = squelch
        self._squelch_slider.set_value(self.squelch)
        self._squelch_text_box.set_value(self.squelch)

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.wxgui_fftsink2_0.set_sample_rate(self.samp_rate)
        self.osmosdr_sink_0.set_sample_rate(self.samp_rate)
        self.analog_sig_source_x_0.set_sampling_freq(self.samp_rate)

    def get_q_offset(self):
        return self.q_offset

    def set_q_offset(self, q_offset):
        self.q_offset = q_offset
        self._q_offset_slider.set_value(self.q_offset)
        self._q_offset_text_box.set_value(self.q_offset)
        self.blocks_add_const_vxx_1.set_k((self.i_offset + 1j * self.q_offset, ))

    def get_phase(self):
        return self.phase

    def set_phase(self, phase):
        self.phase = phase
        self._phase_slider.set_value(self.phase)
        self._phase_text_box.set_value(self.phase)

    def get_magnitude(self):
        return self.magnitude

    def set_magnitude(self, magnitude):
        self.magnitude = magnitude
        self.blocks_multiply_const_vxx_0.set_k(((1 + self.magnitude) + (1j - self.magnitude), ))
        self._magnitude_slider.set_value(self.magnitude)
        self._magnitude_text_box.set_value(self.magnitude)

    def get_i_offset(self):
        return self.i_offset

    def set_i_offset(self, i_offset):
        self.i_offset = i_offset
        self._i_offset_slider.set_value(self.i_offset)
        self._i_offset_text_box.set_value(self.i_offset)
        self.blocks_add_const_vxx_1.set_k((self.i_offset + 1j * self.q_offset, ))

if __name__ == '__main__':
    import ctypes
    import sys
    if sys.platform.startswith('linux'):
        try:
            x11 = ctypes.cdll.LoadLibrary('libX11.so')
            x11.XInitThreads()
        except:
            print "Warning: failed to XInitThreads()"
    parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
    (options, args) = parser.parse_args()
    tb = top_block()
    tb.Start(True)
    tb.Wait()

