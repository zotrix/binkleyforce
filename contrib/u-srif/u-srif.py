#!/usr/local/bin/python

import string
import sys
import os

# Our own libraries
sys.path.append('./lib')
import uconfig
import udbase
import ufido
import utmpl
import unodestat
from uutil import *

USRIF_CONFIG = '/usr/local/etc/u-srif/u-srif.conf'

class freq_report(ufido.message):
	def write_packet(self, pktname):
		self.packet.addr_orig = self.addr_orig
		self.packet.addr_dest = self.addr_dest
		self.packet.write(pktname)
	
	def add_file(self, name, size, desc):
		text = format_file_info(name, size, desc)
		self.append_text(text)
	
	def __init__(self):
		ufido.message.__init__(self)
		self.packet = ufido.packet()
		self.packet.add_message(self)

class srif_file:
	def read_req_list(self):
		yield = []
		fp = open(self.requestlist, 'r')
		line = fp.readline()
		while line:
			line = string.strip(line)
			yield.append(line)
			line = fp.readline()
		fp.close()
		return yield

	def write_resp_list(self, list):
		fp = open(self.responselist, 'w')
		for file in list:
			fp.write('+' + file + '\n')
		fp.close()
	
	def read(self, name):
		fp = open(name, 'r')
		line = fp.readline()
		while line:
			line = string.strip(line)
			args = string.split(line, None, 1)
			if len(args) == 2:
				if string.lower(args[0]) == 'sysop':
					self.sysop = args[1]
				if string.lower(args[0]) == 'aka':
					self.aka.parse(args[1])
				elif string.lower(args[0]) == 'baud':
					self.baud = args[1]
				elif string.lower(args[0]) == 'requestlist':
					self.requestlist = args[1]
				elif string.lower(args[0]) == 'responselist':
					self.responselist = args[1]
				elif string.lower(args[0]) == 'remotestatus':
					self.remotestatus = args[1]
				elif string.lower(args[0]) == 'systemstatus':
					self.systemstatus = args[1]
				elif string.lower(args[0]) == 'site':
					self.site = args[1]
				elif string.lower(args[0]) == 'callerid':
					self.callerid = args[1]
				elif string.lower(args[0]) == 'password':
					self.password = args[1]
				else:
					print "skipping keyword", args[0], "in SRIF"
			line = fp.readline()
		fp.close()
		
	def __init__(self, name):
		self.sysop = ''
		self.aka = ufido.address()
		self.baud = 0
		self.requestlist = ''
		self.responselist = ''
		self.remotestatus = ''
		self.systemstatus = ''
		self.site = ''
		self.callerid = ''
		self.password = ''
		self.read(name)
	
	def remote_addr(self):
		return self.aka
	
	def isprotected(self):
		if string.lower(self.remotestatus) == 'protected':
			return 1
		return 0
	
	def islisted(self):
		if string.lower(self.systemstatus) == 'listed':
			return 1
		return 0

def append_new_file(fileslist, file):
	TotalFiles = TotalFiles + 1
	TotalSize = TotalSize + file.size
	fileslist.append(file.fullname)

##############################
# The main program starts here

# Global variables
yield_list = []

if len(sys.argv) <> 2:
	print 'usage: u-srif <srif file name>'
	sys.exit(1)

# Read configuration
conf = uconfig.Config(USRIF_CONFIG)
# Read SRIF files
srif = srif_file(sys.argv[1])
# Read node's statistic
nodestat = unodestat.nodestat(conf.stat_dbase, srif.aka)
nodestat.get_stat()

# Lookup requested files in the our database
FBase = udbase.filebase(conf.spool_dir)
FBase.open('r')
yield = FBase.get_all(srif.read_req_list())
FBase.close()

# Prepare found files for sending
for file in yield:
	if not file.fake:
		file.stat()
		nodestat.upd_stat(1, file.size)
		yield_list.append(file.fullname)

# Store node's statistic
nodestat.put_stat()

# Send FREQ report?
if conf.send_report:
	# Prepare templates object
	tmpl = utmpl.template()
	tmpl.set(srif=srif, conf=conf, nodestat=nodestat)
	# Setup report object
	report = freq_report()
	report.newmsg(conf.local_address, conf.report_from, \
		srif.aka, srif.sysop, conf.report_subj)
	report.append_line('')
	# Append header
	tmpl.readfile(conf.report_header)
	text = tmpl.process()
	if text:
		report.append_text(text)
	# Append per files statistic
	for file in yield:
		report.add_file(file.name, file.size, file.desc)
	# Append footer
	tmpl.readfile(conf.report_footer)
	text = tmpl.process()
	if text:
		report.append_text(text)
	# Add empty line to the report
	report.append_line('')
	# Create netmail packet with the FREQ report
	pktname = '/var/tmp/12345678.pkt' # XXX
	report.write_packet(pktname)
	# Add packet file to the response files list
	yield_list.append(pktname)

# Dump reponse list
srif.write_resp_list(yield_list)
sys.exit(0)

