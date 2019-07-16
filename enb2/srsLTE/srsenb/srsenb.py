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

FIELD_RNTI 			= 0
FIELD_CQI  			= 1
FIELD_MAC_DL 		= 5
FIELD_MAC_UL 		= 11
FIELD_MAC_TX 		= 4
FIELD_MAC_RX 		= 10
FIELD_LWAAP_DL 		= 14
FIELD_LWAAP_UL 		= 15
FIELD_LWAAP_TX 		= 16
FIELD_LWAAP_RX 		= 17
FIELD_LTE_RATIO 	= 18
FIELD_WIFI_RATIO 	= 19
FIELD_GTPU_DL 		= 20
FIELD_GTPU_UL 		= 21
FIELD_GTPU_TX 		= 22
FIELD_GTPU_RX 		= 23

DATA_FIELD_NAME     = ["CQI", "MAC TX", "MAC RX", "MAC DL", "MAC UL", "LWAAP TX", "LWAAP RX", "LWAAP DL", "LWAAP UL", "LTE Ratio", "WiFi Ratio", "GTPU TX", "GTPU RX", "GTPU DL", "GTPU UL"]
DATA_CQI  			= 0
DATA_MAC_TX 		= 1
DATA_MAC_RX 		= 2
DATA_MAC_DL 		= 3
DATA_MAC_UL 		= 4
DATA_LWAAP_TX 		= 5
DATA_LWAAP_RX 		= 6
DATA_LWAAP_DL 		= 7
DATA_LWAAP_UL 		= 8
DATA_LTE_RATIO 		= 9
DATA_WIFI_RATIO 	= 10
DATA_GTPU_TX 		= 11
DATA_GTPU_RX 		= 12
DATA_GTPU_DL 		= 13
DATA_GTPU_UL 		= 14

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

PROC_START  = manager.Value("proc", False)
TEST_START  = manager.Value("start", False)
PLOT_START  = manager.Value("plot", False)
TEST_RNTI   = manager.Value("rnti", 0)

# Functions
def ishexint(s):
	try:
		int(s, 16)
		return True
	except ValueError:
		return False

def hexint(s):
	try:
		return int(s, 16)
	except ValueError:
		return -1
		
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
	
	if event.key == '1':	# Stop plot and save
		TEST_START.value = False
					
# Figure settings
PLOT_NUM 	= 15
FIG_Y_LABEL_CQI = "CQI"
FIG_Y_LABEL_BRATE = "Bit Rate(Mbps)"
FIG_Y_LABEL_BITS  = "(Mb)"
FIG_Y_LABEL_RATIO = "Ratio"
FIG_X_LABEL = 'Time(sec)'
FIG_LABEL_TXRX = ["MAC", "LWAAP", "GTPU"]
FULL_SCREEN = False
FILE_TYPE = '.png'

# Default directory name is date, file name is time
now = datetime.datetime.now()
directory = now.strftime("enb-%Y-%m-%d")
filename = now.strftime("%H-%M-%s.png")
conf = "local.conf"

# Global variables
clk = 0
pause_time = 0.5
max_time = num(TIME)
maxx = num(PLOT_TIME)
miny = 10
command = []
proc = []
x = [0]
lists = manager.list([[]] * PLOT_NUM)

# Plot all data
def exe_plt(lists):
	while not PROC_START.value:
		# Waiting data process
		time.sleep(pause_time)
		
	while PROC_START.value:
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
			filename = now.strftime("srsenb-%H-%M-%s")
			filetype = FILE_TYPE
			
			fig = plt.figure()
			gs = gridspec.GridSpec(3, 2, width_ratios=[1, 1], height_ratios=[1, 2, 2])
			# fig1 and ax1 plot CQI
			ax1 = plt.subplot(gs[0])
			plt.suptitle("RNTI " + str(TEST_RNTI.value), fontsize=16)
			ax1.set_xlabel(FIG_X_LABEL)
			ax1.set_ylabel(FIG_Y_LABEL_CQI)
			
			# fig2 and ax2 plot ratio
			ax2 = plt.subplot(gs[1])
			ax2.set_xlabel(FIG_X_LABEL)
			ax2.set_ylabel(FIG_Y_LABEL_RATIO)
			
			# fig3 and ax3 plot DL
			ax3 = plt.subplot(gs[2])
			ax3.set_xlabel(FIG_X_LABEL)
			ax3.set_ylabel("DL " + FIG_Y_LABEL_BRATE)
			
			# fig4 and ax4 plot UL
			ax4 = plt.subplot(gs[3])
			ax4.set_xlabel(FIG_X_LABEL)
			ax4.set_ylabel("UL " + FIG_Y_LABEL_BRATE)
			
			# fig5 and ax5 plot TX
			ax5 = plt.subplot(gs[4])
			ax5.set_xlabel(FIG_X_LABEL)
			ax5.set_ylabel("TX " + FIG_Y_LABEL_BITS)
			
			# fig6 and ax6 plot RX
			ax6 = plt.subplot(gs[5])
			ax6.set_xlabel(FIG_X_LABEL)
			ax6.set_ylabel("RX " + FIG_Y_LABEL_BITS)
			
			if FULL_SCREEN:
				mng = plt.get_current_fig_manager()
				mng.resize(*mng.window.maxsize())
			plt.ion()
			fig.canvas.mpl_connect('key_press_event', press)
			
			time.sleep(pause_time)
			
			while True:
				# Plot CQI
				sublst = manager.list(lists[DATA_CQI])	
				plt_y(fig, ax1, sublst, COLOR[0], '')

				# Plot Ratio
				sublst = manager.list(lists[DATA_LTE_RATIO])	
				plt_y(fig, ax2, sublst, COLOR[0], 'LTE', '', 3.0)
				sublst = manager.list(lists[DATA_WIFI_RATIO])	
				plt_y(fig, ax2, sublst, COLOR[1], 'WiFi', '', 2.0)
				
				if clk == 0:
					ax2.legend(bbox_to_anchor=(1.02, 1), loc=2, borderaxespad=0.)
				
				# Plot DL
				sublst = manager.list(lists[DATA_MAC_DL])	
				plt_y(fig, ax3, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_DL])	
				plt_y(fig, ax3, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GTPU_DL])	
				plt_y(fig, ax3, sublst, COLOR[2], 'PDCP', '', 2.0)
				
				# Plot UL
				sublst = manager.list(lists[DATA_MAC_UL])	
				plt_y(fig, ax4, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_UL])	
				plt_y(fig, ax4, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GTPU_UL])	
				plt_y(fig, ax4, sublst, COLOR[2], 'PDCP', '', 2.0)
				
				if clk == 0:
					ax4.legend(bbox_to_anchor=(1.02, 1), loc=2, borderaxespad=0.)
					
				# Plot TX
				sublst = manager.list(lists[DATA_MAC_TX])	
				plt_y(fig, ax5, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_TX])	
				plt_y(fig, ax5, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GTPU_TX])	
				plt_y(fig, ax5, sublst, COLOR[2], 'PDCP', '', 2.0)
				
				# Plot RX
				sublst = manager.list(lists[DATA_MAC_RX])	
				plt_y(fig, ax6, sublst, COLOR[0], 'MAC', '', 3.0)
				sublst = manager.list(lists[DATA_LWAAP_RX])	
				plt_y(fig, ax6, sublst, COLOR[1], 'LWAAP', '', 2.5)
				sublst = manager.list(lists[DATA_GTPU_RX])	
				plt_y(fig, ax6, sublst, COLOR[2], 'PDCP', '', 2.0)
				
				if clk == 0:
					ax6.legend(bbox_to_anchor=(1.02, 1), loc=2, borderaxespad=0.)
				
				plt.pause(pause_time)
				
				clk = clk + 1

				if not TEST_START.value or not PLOT_START.value:
					break
						
			# Check the directory exist, if not, create the directory
			try:
				os.makedirs(directory)
			except OSError as e:
				if e.errno != errno.EEXIST:
					raise
						
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + filetype, bbox_inches='tight')
			plt.close(fig)
			
			################### Plot all data ###################
			##### fig1 plot CQI
			fig, ax = plt.subplots()
			plt.suptitle("RNTI " + str(TEST_RNTI.value), fontsize=16)
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_CQI)
			
			# Plot different subplot
			sublst = manager.list(lists[DATA_MAC_DL])
			ax.plot(sublst, COLOR[0], linewidth=2.0)
			ax.set_xlim(0, len(sublst) - 1)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-cqi" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig2 plot ratio
			fig, ax = plt.subplots()
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel(FIG_Y_LABEL_RATIO)
			
			# Plot different subplot
			sublst = manager.list(lists[DATA_LTE_RATIO])
			ax.plot(sublst, 'b-', linewidth=2.5, label="LTE")
			sublst = manager.list(lists[DATA_WIFI_RATIO])
			ax.plot(sublst, 'r-', linewidth=2.0, label="WiFi")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=2, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-ratio" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig3 plot DL
			fig, ax = plt.subplots()
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel("DL " + FIG_Y_LABEL_BRATE)
			
			# Plot different subplot
			sublst = manager.list(lists[DATA_MAC_DL])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_DL])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GTPU_DL])
			ax.plot(sublst, 'g-', linewidth=2.0, label="PDCP")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-dl" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig4 and ax4 plot UL
			fig, ax = plt.subplots()
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel("UL " + FIG_Y_LABEL_BRATE)
			
			# Plot different subplot
			sublst = manager.list(lists[DATA_MAC_UL])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_UL])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GTPU_UL])
			ax.plot(sublst, 'g-', linewidth=2.0, label="PDCP")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-ul" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig5 and ax5 plot TX
			fig, ax = plt.subplots()
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel("TX " + FIG_Y_LABEL_BITS)
			
			# Plot different subplot
			sublst = manager.list(lists[DATA_MAC_TX])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_TX])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GTPU_TX])
			ax.plot(sublst, 'g-', linewidth=2.0, label="PDCP")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-tx" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### fig6 and ax6 plot RX
			fig, ax = plt.subplots()
			ax.set_xlabel(FIG_X_LABEL)
			ax.set_ylabel("RX " + FIG_Y_LABEL_BITS)
			
			# Plot different subplot
			sublst = manager.list(lists[DATA_MAC_RX])
			ax.plot(sublst, 'b-', linewidth=2.8, label="MAC")
			sublst = manager.list(lists[DATA_LWAAP_RX])
			ax.plot(sublst, 'r-', linewidth=2.4, label="LWAAP")
			sublst = manager.list(lists[DATA_GTPU_RX])
			ax.plot(sublst, 'g-', linewidth=2.0, label="PDCP")
			ax.set_xlim(0, len(sublst) - 1)
			plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3, ncol=3, mode="expand", borderaxespad=0.)
			fig.canvas.draw()
			fig.show()
			
			# Save the figure as PNG
			fig.savefig(directory + "/" + filename + "-rx" + filetype, bbox_inches='tight')
			plt.close(fig)
			
			##### Save all data
			save_data(directory + "/" + filename, DATA_FIELD_NAME)

			PLOT_START.value = False
			
		# Waiting server data
		time.sleep(pause_time)

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
	gtpu_total_tx = 0
	gtpu_total_rx = 0

	with Popen(cmd, stdout=PIPE, bufsize=1, universal_newlines=True) as p:
		for line in p.stdout:
			print("*" + line)
			
			words = re.split("[\t\n, \-\[\]]+", line)
			
			#print(len(words))
			#for widx in range(len(words)):
			#	print("---", widx, ":", words[widx])

			""" iperf3 -c SERVER_IP -B CLIENT_IP --forceflush -t TIME -R
			--- 0 : 47		rnti
			--- 1 : 0.0		cqi		DL
			--- 2 : 0.00	ri
			--- 3 : nan		mcs
			--- 4 : 		tbits 
			--- 5 : 0.0		brate
			--- 6 : 0%		bler(tx errors)
			--- 7 : nan		snr		UL
			--- 8 : 0.0		phr
			--- 9 : nan		mcs
			--- 10 :		rbits
			--- 11 : 0.0		brate
			--- 12 : 0%		bler(rx errors)
			--- 13 : 0.0	bsr
			--- 14 : 0.0	dl		LWAAP
			--- 15 : 0.0	ul
			--- 16 :		tx
			--- 17 :		rx
			--- 18 : 0.0	lte		Ratio
			--- 19 : 0.0	wifi
			--- 20 : 0.0	dl		GTPU
			--- 21 : 0.0	ul
			--- 22 :		tx
			--- 23 :		rx
			--- 24 :
			--- 25 : Msg Length
			"""

			# Check first word is rnti
			if len(words) == 25 and ishexint(words[FIELD_RNTI]):
			
				rnti = hexint(words[FIELD_RNTI])
			
				# Wait last plot end
				if not rnti == TEST_RNTI.value and not PLOT_START.value:
					TEST_START.value = True
					TEST_RNTI.value = rnti
					
					time = 0
					mac_total_tx = 0
					mac_total_rx = 0
					lwaap_total_tx = 0
					lwaap_total_rx = 0
					gtpu_total_tx = 0
					gtpu_total_rx = 0
					for pidx in range(PLOT_NUM):
						subl = manager.list(lst[pidx])
						subl = [0]
						lst[pidx] = subl
			
				if TEST_START.value:
					time = time + 1
					
					# Get CQI
					if isnum(words[FIELD_CQI]):
						cqi = num(words[FIELD_CQI])
						#print("--CQI", cqi)
						
						subl = manager.list(lst[DATA_CQI])
						subl.append(cqi)
						lst[DATA_CQI] = subl
				
					# Get MAC TX
					tx = 0.0
					if not isnum(words[FIELD_MAC_TX]):
						l = len(words[FIELD_MAC_TX])
						r = words[FIELD_MAC_TX][0 : l - 1]
						
						if isnum(r):
							tx = num(r)
							
						# Let brate be the same type (ex: Mb)
						tx = transfer_bit_prefix(tx, words[FIELD_MAC_TX][l - 1])
					else:
						tx = num(words[FIELD_MAC_TX])
						tx = transfer_bit_prefix(tx, 'b')
					
					mac_total_tx = mac_total_tx + tx
					subl = manager.list(lst[DATA_MAC_TX])
					subl.append(mac_total_tx)
					lst[DATA_MAC_TX] = subl
							
					# Get MAC RX
					rx = 0.0
					if not isnum(words[FIELD_MAC_RX]):
						l = len(words[FIELD_MAC_RX])
						r = words[FIELD_MAC_RX][0 : l - 1]
						
						if isnum(r):
							rx = num(r)
							
						# Let brate be the same type (ex: Mb)
						rx = transfer_bit_prefix(rx, words[FIELD_MAC_RX][l - 1])
					else:
						rx = num(words[FIELD_MAC_RX])
						rx = transfer_bit_prefix(rx, 'b')
					
					mac_total_rx = mac_total_rx + rx
					subl = manager.list(lst[DATA_MAC_RX])
					subl.append(mac_total_rx)
					lst[DATA_MAC_RX] = subl
					
					# Get MAC DL brate
					dl = 0.0
					if not isnum(words[FIELD_MAC_DL]):
						l = len(words[FIELD_MAC_DL])
						r = words[FIELD_MAC_DL][0 : l - 1]
						
						if isnum(r):
							dl = num(r)
							
						# Let brate be the same type (ex: Mb)
						dl = transfer_bit_prefix(dl, words[FIELD_MAC_DL][l - 1])
					else:
						dl = num(words[FIELD_MAC_DL])
						dl = transfer_bit_prefix(dl, 'b')
					
					#print("--MAC DL Brate", dl)
					subl = manager.list(lst[DATA_MAC_DL])
					subl.append(dl)
					lst[DATA_MAC_DL] = subl
					
					# Get MAC UL brate
					ul = 0.0
					if not isnum(words[FIELD_MAC_UL]):
						l = len(words[FIELD_MAC_UL])
						r = words[FIELD_MAC_UL][0 : l - 1]
						
						if isnum(r):
							ul = num(r)

						# Let brate be the same type (ex: Mb)
						ul = transfer_bit_prefix(ul, words[FIELD_MAC_UL][l - 1])
					else:
						ul = num(words[FIELD_MAC_UL])
						ul = transfer_bit_prefix(ul, 'b')
					
					#print("--MAC UL Brate", ul)
					subl = manager.list(lst[DATA_MAC_UL])
					subl.append(ul)
					lst[DATA_MAC_UL] = subl
					
					# Get LWAAP TX
					tx = 0.0
					if not isnum(words[FIELD_LWAAP_TX]):
						l = len(words[FIELD_LWAAP_TX])
						r = words[FIELD_LWAAP_TX][0 : l - 1]
						
						if isnum(r):
							tx = num(r)
							
						# Let brate be the same type (ex: Mb)
						tx = transfer_bit_prefix(tx, words[FIELD_LWAAP_TX][l - 1])
					else:
						tx = num(words[FIELD_LWAAP_TX])
						tx = transfer_bit_prefix(tx, 'b')
					
					lwaap_total_tx = lwaap_total_tx + tx
					subl = manager.list(lst[DATA_LWAAP_TX])
					subl.append(lwaap_total_tx)
					lst[DATA_LWAAP_TX] = subl
							
					# Get LWAAP RX
					rx = 0.0
					if not isnum(words[FIELD_LWAAP_RX]):
						l = len(words[FIELD_LWAAP_RX])
						r = words[FIELD_LWAAP_RX][0 : l - 1]
						
						if isnum(r):
							rx = num(r)
							
						# Let brate be the same type (ex: Mb)
						rx = transfer_bit_prefix(rx, words[FIELD_LWAAP_RX][l - 1])
					else:
						rx = num(words[FIELD_LWAAP_RX])
						rx = transfer_bit_prefix(rx, 'b')
					
					lwaap_total_rx = lwaap_total_rx + rx
					subl = manager.list(lst[DATA_LWAAP_RX])
					subl.append(lwaap_total_rx)
					lst[DATA_LWAAP_RX] = subl
					
					# Get LWAAP DL brate
					dl = 0.0
					if not isnum(words[FIELD_LWAAP_DL]):
						l = len(words[FIELD_LWAAP_DL])
						r = words[FIELD_LWAAP_DL][0 : l - 1]
						
						if isnum(r):
							dl = num(r)
							
						# Let brate be the same type (ex: Mb)
						dl = transfer_bit_prefix(dl, words[FIELD_LWAAP_DL][l - 1])
					else:
						dl = num(words[FIELD_LWAAP_DL])
						dl = transfer_bit_prefix(dl, 'b')
					
					#print("--LWAAP DL Brate", dl)
					subl = manager.list(lst[DATA_LWAAP_DL])
					subl.append(dl)
					lst[DATA_LWAAP_DL] = subl
					
					# Get LWAAP UL brate
					ul = 0.0
					if not isnum(words[FIELD_LWAAP_UL]):
						l = len(words[FIELD_LWAAP_UL])
						r = words[FIELD_LWAAP_UL][0 : l - 1]
						
						if isnum(r):
							ul = num(r)
							
						# Let brate be the same type (ex: Mb)
						ul = transfer_bit_prefix(ul, words[FIELD_LWAAP_UL][l - 1])
					else:
						ul = num(words[FIELD_LWAAP_UL])
						ul = transfer_bit_prefix(ul, 'b')
					
					#print("--LWAAP UL Brate", ul)
					subl = manager.list(lst[DATA_LWAAP_UL])
					subl.append(ul)
					lst[DATA_LWAAP_UL] = subl
					
					# Get LTE ratio
					ratio = 0.0
					if isnum(words[FIELD_LTE_RATIO]):
						ratio = num(words[FIELD_LTE_RATIO])
					#print("--LTE ratio", ratio)
					subl = manager.list(lst[DATA_LTE_RATIO])
					subl.append(ratio)
					lst[DATA_LTE_RATIO] = subl
					
					# Get WiFi ratio
					ratio = 0.0
					if isnum(words[FIELD_WIFI_RATIO]):
						ratio = num(words[FIELD_WIFI_RATIO])
					#print("--WiFi ratio", ratio)
					subl = manager.list(lst[DATA_WIFI_RATIO])
					subl.append(ratio)
					lst[DATA_WIFI_RATIO] = subl
					
					# Get GTPU TX
					tx = 0.0
					if not isnum(words[FIELD_GTPU_TX]):
						l = len(words[FIELD_GTPU_TX])
						r = words[FIELD_GTPU_TX][0 : l - 1]
						
						if isnum(r):
							tx = num(r)
							
						# Let brate be the same type (ex: Mb)
						tx = transfer_bit_prefix(tx, words[FIELD_GTPU_TX][l - 1])
					else:
						tx = num(words[FIELD_GTPU_TX])
						tx = transfer_bit_prefix(tx, 'b')
					
					gtpu_total_tx = gtpu_total_tx + tx
					subl = manager.list(lst[DATA_GTPU_TX])
					subl.append(gtpu_total_tx)
					lst[DATA_GTPU_TX] = subl
							
					# Get GTPU RX
					rx = 0.0
					if not isnum(words[FIELD_GTPU_RX]):
						l = len(words[FIELD_GTPU_RX])
						r = words[FIELD_GTPU_RX][0 : l - 1]
						
						if isnum(r):
							rx = num(r)
							
						# Let brate be the same type (ex: Mb)
						rx = transfer_bit_prefix(rx, words[FIELD_GTPU_RX][l - 1])
					else:
						rx = num(words[FIELD_GTPU_RX])
						rx = transfer_bit_prefix(rx, 'b')
					
					gtpu_total_rx = gtpu_total_rx + rx
					subl = manager.list(lst[DATA_GTPU_RX])
					subl.append(gtpu_total_rx)
					lst[DATA_GTPU_RX] = subl
					
					# Get GTPU DL brate
					dl = 0.0
					if not isnum(words[FIELD_GTPU_DL]):
						l = len(words[FIELD_GTPU_DL])
						r = words[FIELD_GTPU_DL][0 : l - 1]
						
						if isnum(r):
							dl = num(r)
							
						# Let brate be the same type (ex: Mb)
						dl = transfer_bit_prefix(dl, words[FIELD_GTPU_DL][l - 1])
					else:
						dl = num(words[FIELD_GTPU_DL])
						dl = transfer_bit_prefix(dl, 'b')
					
					#print("--GTPU DL Brate", dl)
					subl = manager.list(lst[DATA_GTPU_DL])
					subl.append(dl)
					lst[DATA_GTPU_DL] = subl
					
					# Get GTPU UL brate
					ul = 0.0
					if not isnum(words[FIELD_GTPU_UL]):
						l = len(words[FIELD_GTPU_UL])
						r = words[FIELD_GTPU_UL][0 : l - 1]
						
						if isnum(r):
							ul = num(r)
							
						# Let brate be the same type (ex: Mb)
						ul = transfer_bit_prefix(ul, words[FIELD_GTPU_UL][l - 1])
					else:
						ul = num(words[FIELD_GTPU_UL])
						ul = transfer_bit_prefix(ul, 'b')
					
					#print("--GTPU UL Brate", ul)
					subl = manager.list(lst[DATA_GTPU_UL])
					subl.append(ul)
					lst[DATA_GTPU_UL] = subl
				
			elif "Disconnect" in words:
				if TEST_START.value:
					# Rx data is not udp/tcp
					# Test ending
					TEST_START.value = False
					print("Rx end")

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
		opts, args = getopt.getopt(argv, "hc:fp:", ["help", "directory=", "fullscrenn"])
	except getopt.GetoptError:
		print ('srsenb.py [-d <directory> -f -p <plot_time>]')
		sys.exit(2)
	for opt, arg in opts:
		if opt == '-h':
			print ('srsenb.py [-d <directory> -f -p <plot_time>]')
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
	command = ["sudo", "srsenb", conf]
		
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
