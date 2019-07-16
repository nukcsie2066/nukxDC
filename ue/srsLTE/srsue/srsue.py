#!/usr/bin/env python3
from subprocess import Popen, PIPE
from multiprocessing import Process, Manager, Lock
from matplotlib.widgets import Slider, Button, RadioButtons
import multiprocessing
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np
import pylab
import re
import time
import sys
import getopt
import errno
import datetime
import os
import xlwt

# Common settings
TIME		= "30"
PLOT_TIME   = "60"
manager		= Manager()
mutex 		= Lock()
bit_prefix      	= ['G', 'M', 'k', 'b']
byte_prefix     	= ['G', 'M', 'K', 'B']
time_prefix			= ['s', 'ms']
DEFAULT_BIT_PREFIX  = 1 # Mb
DEFAULT_BYTE_PREFIX = 2 # KB
DEFAULT_TIME_PREFIX = 1 # ms
DEFAULT_AUTO_FINISH = True

FIELD_RSRP				= 1
FIELD_MCS_DL			= 4
FIELD_MAC_RX			= 7
FIELD_MAC_DL 			= 8
FIELD_MAC_DL_ERROR		= 9
FIELD_MCS_UL			= 10
FIELD_MAC_TX			= 12
FIELD_MAC_UL 			= 13
FIELD_MAC_UL_ERROR		= 14
FIELD_LWAAP_TX 			= 15
FIELD_LWAAP_RX 			= 16
FIELD_LWAAP_DL 			= 17
FIELD_LWAAP_UL 			= 18
FIELD_GW_TX 			= 19
FIELD_GW_RX 			= 20
FIELD_GW_DL 			= 21
FIELD_GW_UL 			= 22
FIELD_PDCP_REORDER		= 23
FIELD_PDCP_DELAYED		= 24
FIELD_PDCP_EXPIRED		= 25
FIELD_PDCP_DUPLICATE	= 26
FIELD_PDCP_OUTOFORDER	= 27
FIELD_PDCP_REPORT	= 28

DATA_MCS_DL			= 0
DATA_MAC_RX			= 1
DATA_MAC_DL 		= 2
DATA_MAC_DL_ERROR	= 3
DATA_MCS_UL			= 4
DATA_MAC_TX			= 5
DATA_MAC_UL 		= 6
DATA_MAC_UL_ERROR	= 7
DATA_LWAAP_TX 		= 8
DATA_LWAAP_RX 		= 9
DATA_LWAAP_DL 		= 10
DATA_LWAAP_UL 		= 11
DATA_GW_TX 			= 12
DATA_GW_RX 			= 13
DATA_GW_DL 			= 14
DATA_GW_UL 			= 15
DATA_PDCP_REORDER	= 16
DATA_PDCP_DELAYED	= 17
DATA_PDCP_EXPIRED	= 18
DATA_PDCP_DUPLICATE	= 19
DATA_PDCP_OUTOFORDER= 20
DATA_PDCP_REPORT	= 21

# Currently not used
DEFAULT_COLOR		= ['b', 'r', 'g']
DEFAULT_LABEL		= "LWA"
DEFAULT_MARK		= ":"
COLOR     = DEFAULT_COLOR
LABEL     = DEFAULT_LABEL

# Default connection settings
DEFAULT_CLIENT_IP 	= "127.0.0.1"
DEFAULT_SERVER_IP 	= "127.0.0.1"
DEFAULT_BANDWIDTH	= "10M"
DEFAULT_PKT_LENGTH	= "1K"

# Coneection settings
SERVER_IP	= DEFAULT_SERVER_IP
CLIENT_IP	= DEFAULT_CLIENT_IP
BANDWIDTH	= DEFAULT_BANDWIDTH
PKT_LENGTH	= DEFAULT_PKT_LENGTH
AUTO_FINISH = DEFAULT_AUTO_FINISH

DATA_FIELD_NAME     = ["MCS DL", "MAC RX", "MAC DL", "MAC DL ERROR Rate", "MCS UL", "MAC TX", "MAC UL", "MAC UL Error Rate", "LWAAP TX", "LWAAP RX", "LWAAP DL", "LWAAP UL", "GW TX", "GW RX", "GW DL", "GW UL", "Reorder", "Delay", "Expired", "Duplicate", "Out-of-order", "Report"]

PROC_START  = manager.Value("proc", False)
TEST_START  = manager.Value("start", False)
PLOT_START  = manager.Value("plot", False)
TEST_IP     = manager.Value("ip", "172.16.0.1")

# Functions
def isnum(s):
	try:
		float(s)
		return True
	except ValueError:
		return False

def num(s):
    try:
        return float(s)
    except ValueError:
        return -1

def press(event):
	print('press', event.key)
	sys.stdout.flush()
	if event.key == '1':
		PROC_START.value = False
					
# Figure settings
PLOT_NUM 	= 22
FIG_Y_LABEL_MCS = "MCS"
FIG_Y_LABEL_BRATE = "Bit Rate(Mbps)"
FIG_Y_LABEL_ERROR = "Error Rate(%)"
FIG_Y_LABEL_RX = "Receive(Mb)"
FIG_Y_LABEL_TX = "Transmit(Mb)"
FIG_X_LABEL = 'Time(sec)'
FIG_LABEL_TXRX = ["MAC", "LWAAP", "GW"]
FULL_SCREEN = False
FILE_TYPE = '.png'

# Default directory name is date, file name is time
now = datetime.datetime.now()
directory = now.strftime("ue-%Y-%m-%d")
conf = "ue.conf"

# Global variables
clk = 0
pause_time = 0.3
max_time = num(TIME)
maxx = num(PLOT_TIME)
miny = 10
command = []
proc = []
x = [0]
lists = manager.list([[]] * PLOT_NUM)

# Plot all data
def exe_plt(lists):
	
	while True:
	
		# Server start test
		if TEST_START.value and not PLOT_START.value:
			# Start plot
			PLOT_START.value = True
			print("Plot start")
			
			# Init variables
			clk = 0
			times = 0
			# Default file name is time
			now = datetime.datetime.now()
			filename = now.strftime("srsue-%H-%M-%s")
			filetype = FILE_TYPE
			"""
			fig = plt.figure()
			gs = gridspec.GridSpec(3, 2, width_ratios=[1, 1], height_ratios=[1, 2, 2])

			# Put ip to figure title
			plt.suptitle(TEST_IP.value, fontsize=12)

			# fig1 and ax1 plot MCS
			ax1 = plt.subplot(gs[0])
			ax1.set_xlabel(FIG_X_LABEL)
			ax1.set_ylabel(FIG_Y_LABEL_MCS)
			
			# fig2 and ax2 plot ratio
			ax2 = plt.subplot(gs[1])
			ax2.set_xlabel(FIG_X_LABEL)
			ax2.set_ylabel(FIG_Y_LABEL_ERROR)
			
			# fig3 and ax3 plot DL
			ax3 = plt.subplot(gs[2])
			ax3.set_xlabel(FIG_X_LABEL)
			ax3.set_ylabel("DL" + FIG_Y_LABEL_BRATE)
			
			# fig4 and ax4 plot UL
			ax4 = plt.subplot(gs[3])
			ax4.set_xlabel(FIG_X_LABEL)
			ax4.set_ylabel("UL" + FIG_Y_LABEL_BRATE)
			
			# fig5 and ax5 plot RX
			ax5 = plt.subplot(gs[4])
			ax5.set_xlabel(FIG_X_LABEL)
			ax5.set_ylabel(FIG_Y_LABEL_RX)
			
			# fig6 and ax6 plot TX
			ax6 = plt.subplot(gs[5])
			ax6.set_xlabel(FIG_X_LABEL)
			ax6.set_ylabel(FIG_Y_LABEL_TX)
			
			if FULL_SCREEN:
				mng = plt.get_current_fig_manager()
				mng.resize(*mng.window.maxsize())
			plt.ion()
			fig.canvas.mpl_connect('key_press_event', press)
			"""
			time.sleep(pause_time)
			
			while True:
				"""# Plot MCS
				sublst = manager.list(lists[DATA_MCS_DL])	
				plt_y(fig, ax1, sublst, COLOR[0], 'DL', '', 3.0)
				sublst = manager.list(lists[DATA_MCS_UL])	
				plt_y(fig, ax1, sublst, COLOR[1], 'UL', '', 2.0)

				# Plot Ratio
				sublst = manager.list(lists[DATA_MAC_DL_ERROR])	
				plt_y(fig, ax2, sublst, COLOR[0], 'DL', '', 3.0)
				sublst = manager.list(lists[DATA_MAC_UL_ERROR])	
				plt_y(fig, ax2, sublst, COLOR[1], 'UL', '', 2.0)
				
				if clk == 0:
					ax2.legend(bbox_to_anchor=(1.02, 1), loc=2, borderaxespad=0.)
				
				# Plot DL
				sublst = manager.list(lists[DATA_MAC_DL])	
				plt_y(fig, ax3, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_DL])	
				plt_y(fig, ax3, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GW_DL])	
				plt_y(fig, ax3, sublst, COLOR[2], 'GW', '', 2.0)
				
				# Plot UL
				sublst = manager.list(lists[DATA_MAC_UL])	
				plt_y(fig, ax4, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_UL])	
				plt_y(fig, ax4, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GW_UL])	
				plt_y(fig, ax4, sublst, COLOR[2], 'GW', '', 2.0)
				
				if clk == 0:
					ax4.legend(bbox_to_anchor=(1.02, 1), loc=2, borderaxespad=0.)
					
				# Plot RX
				sublst = manager.list(lists[DATA_MAC_RX])	
				plt_y(fig, ax5, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_RX])	
				plt_y(fig, ax5, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GW_RX])	
				plt_y(fig, ax5, sublst, COLOR[2], 'GW', '', 2.0)
				
				# Plot TX
				sublst = manager.list(lists[DATA_MAC_TX])
				plt_y(fig, ax6, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_TX])
				plt_y(fig, ax6, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GW_TX])
				plt_y(fig, ax6, sublst, COLOR[2], 'GW', '', 2.0)
				
				if clk == 0:
					ax6.legend(bbox_to_anchor=(1.02, 1), loc=2, borderaxespad=0.)
				"""
				plt.pause(pause_time)
				
				clk = clk + 1

				if not TEST_START.value or not PROC_START.value:
					break
						
			# Check the directory exist, if not, create the directory
			try:
				os.makedirs(directory)
			except OSError as e:
				if e.errno != errno.EEXIST:
					raise
			"""			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + filetype, bbox_inches='tight')
			plt.close(fig)
			"""
			################### Plot all data ###################
			##### fig1 plot MCS
			fig, ax = plt.subplots()
			plt.suptitle(TEST_IP.value, fontsize=10)
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_MCS)
			
			sublst = manager.list(lists[DATA_MCS_DL])
			ax.plot(sublst, 'b-', linewidth=2.5, label="DL")
			sublst = manager.list(lists[DATA_MCS_UL])
			ax.plot(sublst, 'r-', linewidth=2.0, label="UL")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=2, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-mcs" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig2 plot error rate
			fig, ax = plt.subplots()
			plt.suptitle(TEST_IP.value, fontsize=10)
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_ERROR)
			
			sublst = manager.list(lists[DATA_MAC_DL_ERROR])
			ax.plot(sublst, 'b-', linewidth=2.5, label="DL")
			sublst = manager.list(lists[DATA_MAC_UL_ERROR])
			ax.plot(sublst, 'r-', linewidth=2.0, label="UL")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=2, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-error" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig3 plot DL
			fig, ax = plt.subplots()
			plt.suptitle(TEST_IP.value, fontsize=10)
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_BRATE)
			
			sublst = manager.list(lists[DATA_MAC_DL])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_DL])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GW_DL])
			ax.plot(sublst, 'g-', linewidth=2.0, label="GW")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-dl" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig4 plot UL
			fig, ax = plt.subplots()
			plt.suptitle(TEST_IP.value, fontsize=10)
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_BRATE)
			
			sublst = manager.list(lists[DATA_MAC_UL])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_UL])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GW_UL])
			ax.plot(sublst, 'g-', linewidth=2.0, label="GW")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-ul" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig5 plot RX
			fig, ax = plt.subplots()
			plt.suptitle(TEST_IP.value, fontsize=10)
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_RX)
			
			sublst = manager.list(lists[DATA_MAC_RX])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_RX])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GW_RX])
			ax.plot(sublst, 'g-', linewidth=2.0, label="GW")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-rx" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig6 plot TX
			fig, ax = plt.subplots()
			plt.suptitle(TEST_IP.value, fontsize=10)
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_TX)
			
			sublst = manager.list(lists[DATA_MAC_TX])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_TX])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GW_TX])
			ax.plot(sublst, 'g-', linewidth=2.0, label="GW")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-tx" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### Save all data
			save_data(directory + "/" + filename, DATA_FIELD_NAME)
			
			PLOT_START.value = False
			
		# Waiting server data
		time.sleep(pause_time)
		
		if not PROC_START.value:
			break

def plt_y(fig, ax, ys, colors, lbl, mrk = "-", lw = 2.0):
	l = len(ys)
	if l == 0:
		return
	
	y = max(max(ys), miny)
	if l <= maxx:
		ax.set_xlim(0, maxx)
		plt_y_dynamic(fig, ax, ys, colors, lbl, mrk, lw)
	else:
		ax.set_xlim(l - maxx - 1, l - 1)
		plt_y_dynamic(fig, ax, ys, colors, lbl, mrk, lw)
		
def plt_xy(fig, ax, xs, ys, colors, lbl, mrk = "-"):
	x = xs[len(xs) - 1]
	y = max(max(ys), miny)
	if x <= maxx:
		ax.set_xlim(0, maxx)
		plt_dynamic(fig, ax, xs, ys, colors, lbl, mrk)
	else:
		ax.set_xlim(x - maxx, x)
		plt_dynamic(fig, ax, xs[x - maxx - 1 : x - 1], ys[x - maxx - 1 : x - 1], colors, lbl, mrk)
		
def plt_dynamic(fig, ax, x, y, color, lbl, mrk):
	ax.plot(x, y, color + mrk, label=lbl)
	#fig.canvas.flush_events()
	fig.canvas.draw()
	fig.show()

def plt_y_dynamic(fig, ax, y, color, lbl, mrk, lw):
	ax.plot(y, color + mrk, label=lbl, linewidth=lw)
	#fig.canvas.flush_events()
	fig.canvas.draw()
	fig.show()
	#plt.show()
	
def transfer_bit_prefix(value, prefix):
	pidx = 0
	for pidx in range(len(bit_prefix)):
		if bit_prefix[pidx] == prefix:
			break
	
	while pidx < DEFAULT_BIT_PREFIX:
		value = value * 1000
		pidx = pidx + 1
						
	while pidx < len(bit_prefix) and pidx > DEFAULT_BIT_PREFIX:
		value = value / 1000
		pidx = pidx - 1

	return value

def transfer_byte_prefix(value, prefix):
	pidx = 0
	for pidx in range(len(byte_prefix)):
		if bit_prefix[pidx] == prefix:
			break
	
	while pidx < DEFAULT_BYTE_PREFIX:
		value = value * 1000
		pidx = pidx + 1
						
	while pidx < len(byte_prefix) and pidx > DEFAULT_BYTE_PREFIX:
		value = value / 1000
		pidx = pidx - 1

	return value

def exe_cmd(cmd, lst, idx):
	PROC_START.value = True
	mac_total_tx = 0
	mac_total_rx = 0
	lwaap_total_tx = 0
	lwaap_total_rx = 0
	gw_total_tx = 0
	gw_total_rx = 0

	with Popen(cmd, stdout=PIPE, bufsize=1, universal_newlines=True) as p:
		for line in p.stdout:
			print("*" + line)
			
			words = re.split("[\t\n, \-\[\]]+", line)
			
			#for widx in range(len(words)):
			#	print("---", widx, ":", words[widx])

			"""
			--- 0 : 47		rsrp	Signal
			--- 1 : 0.0		pathloss
			--- 2 : 0.00	cfo
			--- 3 : nan		mcs		DL
			--- 4 : 0.0		snr
			--- 5 : 0%		turbo
			--- 6 :			bits
			--- 7 : nan		brate
			--- 8 : 0.0		bler
			--- 9 : nan		mcs		UL
			--- 10 : 0.0	buff
			--- 11 :		bits
			--- 12 : 0%		brate
			--- 13 : 0.0	bler
			--- 14 :		tx		LWAAP
			--- 15 :		rx
			--- 16 : 0.0	dl
			--- 17 : 0.0	ul
			--- 18 :		tx		GW
			--- 19 :		rx
			--- 20 : 0.0	dl
			--- 21 : 0.0	ul
			--- 22 :		reorder	PDCP
			--- 23 :		delayed
			--- 24 :		expired
			--- 25 :		duplicate
			--- 26 :		Out-of-order
			--- 27 :		report
			--- 28 : Msg Length
			"""

			# Check first word is rsrp
			if TEST_START.value and len(words) > 27 and isnum(words[FIELD_RSRP]):
			
				rsrp = num(words[FIELD_RSRP])
			
				time = time + 1
				
				### Get DL MCS
				if isnum(words[FIELD_MCS_DL]):
					mcs = num(words[FIELD_MCS_DL])
					#print("--DL MCS", mcs)
					
					subl = manager.list(lst[DATA_MCS_DL])
					subl.append(mcs)
					lst[DATA_MCS_DL] = subl
			
				### Get MAC RX
				rx = 0.0
				if not isnum(words[FIELD_MAC_RX]) and len(words[FIELD_MAC_RX]) > 1:
					l = len(words[FIELD_MAC_RX])
					r = words[FIELD_MAC_RX][0 : l - 1]
					
					if isnum(r):
						rx = num(r)
						
					# Let brate be the same type (ex: Mb)
					rx = transfer_bit_prefix(rx, words[FIELD_MAC_RX][l - 1])
				elif isnum(words[FIELD_MAC_RX]):
					rx = num(words[FIELD_MAC_RX])
					rx = transfer_bit_prefix(rx, 'b')
				
				#print("--MAC RX Brate", rx)
				mac_total_rx = mac_total_rx + rx
				subl = manager.list(lst[DATA_MAC_RX])
				subl.append(mac_total_rx)
				lst[DATA_MAC_RX] = subl
					
				### Get MAC DL brate
				dl = 0.0
				if not isnum(words[FIELD_MAC_DL]) and len(words[FIELD_MAC_DL]) > 1:
					l = len(words[FIELD_MAC_DL])
					r = words[FIELD_MAC_DL][0 : l - 1]
					
					if isnum(r):
						dl = num(r)
						
					# Let brate be the same type (ex: Mb)
					dl = transfer_bit_prefix(dl, words[FIELD_MAC_DL][l - 1])
				elif isnum(words[FIELD_MAC_DL]):
					dl = num(words[FIELD_MAC_DL])
					dl = transfer_bit_prefix(dl, 'b')
				
				#print("--MAC DL Brate", dl)
				subl = manager.list(lst[DATA_MAC_DL])
				subl.append(dl)
				lst[DATA_MAC_DL] = subl
				
				### Get MAC DL error rate
				er = 0.0
				if not isnum(words[FIELD_MAC_DL_ERROR]) and len(words[FIELD_MAC_DL_ERROR]) > 1:
					l = len(words[FIELD_MAC_DL_ERROR])
					r = words[FIELD_MAC_DL_ERROR][0 : l - 1]
					
					if isnum(r):
						er = num(r)
				
				#print("--MAC DL Brate", dl)
				subl = manager.list(lst[DATA_MAC_DL_ERROR])
				subl.append(er)
				lst[DATA_MAC_DL_ERROR] = subl
				
				### Get UL MCS
				if isnum(words[FIELD_MCS_UL]):
					mcs = num(words[FIELD_MCS_UL])
					#print("--UL MCS", mcs)
					
					subl = manager.list(lst[DATA_MCS_UL])
					subl.append(mcs)
					lst[DATA_MCS_UL] = subl
				
				### Get MAC TX
				tx = 0.0
				if not isnum(words[FIELD_MAC_TX]) and len(words[FIELD_MAC_TX]) > 1:
					l = len(words[FIELD_MAC_TX])
					r = words[FIELD_MAC_TX][0 : l - 1]
					
					if isnum(r):
						tx = num(r)
						
					# Let brate be the same type (ex: Mb)
					tx = transfer_bit_prefix(tx, words[FIELD_MAC_TX][l - 1])
				elif isnum(words[FIELD_MAC_DL_ERROR]):
					tx = num(words[FIELD_MAC_TX])
					tx = transfer_bit_prefix(tx, 'b')
				
				#print("--MAC TX Brate", tx)
				mac_total_tx = mac_total_tx + tx
				subl = manager.list(lst[DATA_MAC_TX])
				subl.append(mac_total_tx)
				lst[DATA_MAC_TX] = subl
					
				### Get MAC UL brate
				ul = 0.0
				if not isnum(words[FIELD_MAC_UL]) and len(words[FIELD_MAC_UL]) > 1:
					l = len(words[FIELD_MAC_UL])
					r = words[FIELD_MAC_UL][0 : l - 1]
					
					if isnum(r):
						ul = num(r)

					# Let brate be the same type (ex: Mb)
					ul = transfer_bit_prefix(ul, words[FIELD_MAC_UL][l - 1])
				elif isnum(words[FIELD_MAC_UL]):
					ul = num(words[FIELD_MAC_UL])
					ul = transfer_bit_prefix(ul, 'b')
				
				#print("--MAC UL Brate", ul)
				subl = manager.list(lst[DATA_MAC_UL])
				subl.append(ul)
				lst[DATA_MAC_UL] = subl
				
				### Get MAC UL error rate
				er = 0.0
				if not isnum(words[FIELD_MAC_UL_ERROR]) and len(words[FIELD_MAC_UL_ERROR]) > 1:
					l = len(words[FIELD_MAC_UL_ERROR])
					r = words[FIELD_MAC_UL_ERROR][0 : l - 1]
					
					if isnum(r):
						er = num(r)
				
				#print("--MAC UL Error rate", er)
				subl = manager.list(lst[DATA_MAC_UL_ERROR])
				subl.append(er)
				lst[DATA_MAC_UL_ERROR] = subl
				
				### Get LWAAP TX
				tx = 0.0
				if not isnum(words[FIELD_LWAAP_TX]) and len(words[FIELD_LWAAP_TX]) > 1:
					l = len(words[FIELD_LWAAP_TX])
					r = words[FIELD_LWAAP_TX][0 : l - 1]
					
					if isnum(r):
						tx = num(r)
						
					# Let brate be the same type (ex: Mb)
					tx = transfer_bit_prefix(tx, words[FIELD_LWAAP_TX][l - 1])
				elif isnum(words[FIELD_LWAAP_TX]):
					tx = num(words[FIELD_LWAAP_TX])
					tx = transfer_bit_prefix(tx, 'b')
				
				#print("--LWAAP TX Brate", tx)
				lwaap_total_tx = lwaap_total_tx + tx
				subl = manager.list(lst[DATA_LWAAP_TX])
				subl.append(lwaap_total_tx)
				lst[DATA_LWAAP_TX] = subl
				
				### Get LWAAP RX
				rx = 0.0
				if not isnum(words[FIELD_LWAAP_RX]) and len(words[FIELD_LWAAP_RX]) > 1:
					l = len(words[FIELD_LWAAP_RX])
					r = words[FIELD_LWAAP_RX][0 : l - 1]
					
					if isnum(r):
						rx = num(r)
						
					# Let brate be the same type (ex: Mb)
					rx = transfer_bit_prefix(rx, words[FIELD_LWAAP_RX][l - 1])
				elif isnum(words[FIELD_LWAAP_RX]):
					rx = num(words[FIELD_LWAAP_RX])
					rx = transfer_bit_prefix(rx, 'b')
				
				#print("--LWAAP RX Brate", rx)
				lwaap_total_rx = lwaap_total_rx + rx
				subl = manager.list(lst[DATA_LWAAP_RX])
				subl.append(lwaap_total_rx)
				lst[DATA_LWAAP_RX] = subl
				
				### Get LWAAP DL brate
				dl = 0.0
				if not isnum(words[FIELD_LWAAP_DL]) and len(words[FIELD_LWAAP_DL]) > 1:
					l = len(words[FIELD_LWAAP_DL])
					r = words[FIELD_LWAAP_DL][0 : l - 1]
					
					if isnum(r):
						dl = num(r)
						
					# Let brate be the same type (ex: Mb)
					dl = transfer_bit_prefix(dl, words[FIELD_LWAAP_DL][l - 1])
				elif isnum(words[FIELD_LWAAP_DL]):
					dl = num(words[FIELD_LWAAP_DL])
					dl = transfer_bit_prefix(dl, 'b')
				
				#print("--LWAAP DL Brate", dl)
				subl = manager.list(lst[DATA_LWAAP_DL])
				subl.append(dl)
				lst[DATA_LWAAP_DL] = subl
				
				### Get LWAAP UL brate
				ul = 0.0
				if not isnum(words[FIELD_LWAAP_UL]) and len(words[FIELD_LWAAP_UL]) > 1:
					l = len(words[FIELD_LWAAP_UL])
					r = words[FIELD_LWAAP_UL][0 : l - 1]
					
					if isnum(r):
						ul = num(r)
						
					# Let brate be the same type (ex: Mb)
					ul = transfer_bit_prefix(ul, words[FIELD_LWAAP_UL][l - 1])
				elif isnum(words[FIELD_LWAAP_UL]):
					ul = num(words[FIELD_LWAAP_UL])
					ul = transfer_bit_prefix(ul, 'b')
				
				#print("--LWAAP UL Brate", ul)
				subl = manager.list(lst[DATA_LWAAP_UL])
				subl.append(ul)
				lst[DATA_LWAAP_UL] = subl
				
				### Get GW TX
				tx = 0.0
				if not isnum(words[FIELD_GW_TX]) and len(words[FIELD_GW_TX]) > 1:
					l = len(words[FIELD_GW_TX])
					r = words[FIELD_GW_TX][0 : l - 1]
					
					if isnum(r):
						tx = num(r)
						
					# Let brate be the same type (ex: Mb)
					tx = transfer_bit_prefix(tx, words[FIELD_GW_TX][l - 1])
				elif isnum(words[FIELD_GW_TX]):
					tx = num(words[FIELD_GW_TX])
					tx = transfer_bit_prefix(tx, 'b')
				
				#print("--GW TX Brate", tx)
				gw_total_tx = gw_total_tx + tx
				subl = manager.list(lst[DATA_GW_TX])
				subl.append(gw_total_tx)
				lst[DATA_GW_TX] = subl
				
				### Get GW RX
				rx = 0.0
				if not isnum(words[FIELD_GW_RX]) and len(words[FIELD_GW_RX]) > 1:
					l = len(words[FIELD_GW_RX])
					r = words[FIELD_GW_RX][0 : l - 1]
					
					if isnum(r):
						rx = num(r)
						
					# Let brate be the same type (ex: Mb)
					rx = transfer_bit_prefix(rx, words[FIELD_GW_RX][l - 1])
				elif isnum(words[FIELD_GW_RX]):
					rx = num(words[FIELD_GW_RX])
					rx = transfer_bit_prefix(rx, 'b')
				
				#print("--GW RX Brate", rx)
				gw_total_rx = gw_total_rx + rx
				subl = manager.list(lst[DATA_GW_RX])
				subl.append(gw_total_rx)
				lst[DATA_GW_RX] = subl
				
				### Get GW DL brate
				dl = 0.0
				if not isnum(words[FIELD_GW_DL]) and len(words[FIELD_GW_DL]) > 1:
					l = len(words[FIELD_GW_DL])
					r = words[FIELD_GW_DL][0 : l - 1]
					
					if isnum(r):
						dl = num(r)
						
					# Let brate be the same type (ex: Mb)
					dl = transfer_bit_prefix(dl, words[FIELD_GW_DL][l - 1])
				elif isnum(words[FIELD_GW_DL]):
					dl = num(words[FIELD_GW_DL])
					dl = transfer_bit_prefix(dl, 'b')
				
				#print("--GW DL Brate", dl)
				subl = manager.list(lst[DATA_GW_DL])
				subl.append(dl)
				lst[DATA_GW_DL] = subl
				
				### Get GW UL brate
				ul = 0.0
				if not isnum(words[FIELD_GW_UL]) and len(words[FIELD_GW_UL]) > 1:
					l = len(words[FIELD_GW_UL])
					r = words[FIELD_GW_UL][0 : l - 1]
					
					if isnum(r):
						ul = num(r)
						
					# Let brate be the same type (ex: Mb)
					ul = transfer_bit_prefix(ul, words[FIELD_GW_UL][l - 1])
				elif isnum(words[FIELD_GW_UL]):
					ul = num(words[FIELD_GW_UL])
					ul = transfer_bit_prefix(ul, 'b')
				
				#print("--GW UL Brate", ul)
				subl = manager.list(lst[DATA_GW_UL])
				subl.append(ul)
				lst[DATA_GW_UL] = subl
				
				### Get PDCP reordering cnt
				reorder = 0.0
				if isnum(words[FIELD_PDCP_REORDER]):
					reorder = num(words[FIELD_PDCP_REORDER])
				
				#print("--PDCP reordering cnt", reorder)
				subl = manager.list(lst[DATA_PDCP_REORDER])
				subl.append(reorder)
				lst[DATA_PDCP_REORDER] = subl
				
				### Get PDCP delayed cnt
				delay = 0.0
				if isnum(words[FIELD_PDCP_DELAYED]):
					delay = num(words[FIELD_PDCP_DELAYED])
				
				#print("--PDCP delayed cnt", delay)
				subl = manager.list(lst[DATA_PDCP_DELAYED])
				subl.append(delay)
				lst[DATA_PDCP_DELAYED] = subl
				
				### Get PDCP expired cnt
				expired = 0.0
				if isnum(words[FIELD_PDCP_EXPIRED]):
					expired = num(words[FIELD_PDCP_EXPIRED])
				
				#print("--PDCP expired cnt", expired)
				subl = manager.list(lst[DATA_PDCP_EXPIRED])
				subl.append(expired)
				lst[DATA_PDCP_EXPIRED] = subl
				
				### Get PDCP duplicate cnt
				dup = 0.0
				if isnum(words[FIELD_PDCP_DUPLICATE]):
					dup = num(words[FIELD_PDCP_DUPLICATE])
				
				#print("--PDCP duplicate cnt", dup)
				subl = manager.list(lst[DATA_PDCP_DUPLICATE])
				subl.append(dup)
				lst[DATA_PDCP_DUPLICATE] = subl
				
				### Get PDCP outoforder cnt
				outoforder = 0.0
				if isnum(words[FIELD_PDCP_OUTOFORDER]):
					outoforder = num(words[FIELD_PDCP_OUTOFORDER])
				
				#print("--PDCP outoforder cnt", outoforder)
				subl = manager.list(lst[DATA_PDCP_OUTOFORDER])
				subl.append(outoforder)
				lst[DATA_PDCP_OUTOFORDER] = subl
				
				### Get PDCP report cnt
				report = 0.0
				if isnum(words[FIELD_PDCP_REPORT]):
					report = num(words[FIELD_PDCP_REPORT])
				
				#print("--PDCP report cnt", report)
				subl = manager.list(lst[DATA_PDCP_REPORT])
				subl.append(report)
				lst[DATA_PDCP_REPORT] = subl

			elif "trace." in words:
				if not TEST_START.value:
					TEST_START.value = True
					
					time = 0
					for pidx in range(PLOT_NUM):
						subl = manager.list(lst[pidx])
						subl = [0]
						lst[pidx] = subl
			elif "IP:" in words:
				TEST_IP.value = words[4]
			#elif "disconnected" in words:
			#	if TEST_START.value:
					# Rx data is not udp/tcp
					# Test ending
			#		TEST_START.value = False
			#		print("Rx end")
			#	break
	print("Process", idx, "end")
	TEST_START.value = False
	PROC_START.value = False

def save_data(filename, col_name):
    book = xlwt.Workbook()
    sh = book.add_sheet('Sheet 1', cell_overwrite_ok=True)

    print(len(col_name))
    for i, name in enumerate(col_name):
        print(i, name)
        sh.write(0, i, name)

    for i, lst in enumerate(lists):
        for j, val in enumerate(lst, 1):
            sh.write(j, i, val)

    if ".xlsx" in filename:
        book.save(filename)
    else:
        book.save(filename + ".xls")

# Get args
def args(argv):
	
	global PLOT_TIME, FULL_SCREEN, directory, conf
	opt_bw = False
	opt_pkt_len = False
	
	try:
		opts, args = getopt.getopt(argv, "hc:fp:", ["help", "conf=", "directory=", "fullscrenn"])
	except getopt.GetoptError:
		print ('srsue.py [-d <directory> -f -p <plot_time>]')
		sys.exit(2)
	for opt, arg in opts:
		if opt == '-h':
			print ('srsue.py [-d <directory> -f -p <plot_time>]')
			sys.exit()
		elif opt in ("-c", "--conf"):
			conf = arg
		elif opt in ("-d", "--directory"):
			directory = arg
		elif opt in ("-f", "--fullscreen"):
			FULL_SCREEN = True
		elif opt in ("-p", "--plot_time"):
			PLOT_TIME = arg

	global command
	command = ["sudo", "srsue", conf]
		
	print(command)
			
# Main
def main():
	# Execute iperf processes
	proc.append(multiprocessing.Process(target=exe_cmd, args=(command, lists, 0)))
	
	# Plot network capacity process	
	proc.append(multiprocessing.Process(target=exe_plt, args=(lists, )))
	
	[p.start() for p in proc]
	[p.join() for p in proc]

if __name__ == '__main__':
	args(sys.argv[1:])
	main()
