
import gdbm
import string
import os
import ufido

ALIAS_TYPE_NORMAL = 1	# Traditional aliase
ALIAS_TYPE_MAGIC  = 2	# "Magic" alias

# TODO: These functions must generate an exception in case of errors
def get_bool(str):
	str = string.lower(str)
	if str == 'yes' or str == 'true':
		return 1
	elif str == 'no' or str == 'false':
		return 0
	return None

def get_size(str):
	# TODO: support nice size formats like 64M, 10G
	return str

def get_alias(str, type):
	args = string.split(str)
	if len(args) < 2 or len(args) > 3:
		return None
	if len(args) == 3:
		passwd = args[2]
	else:
		passwd = None
	return Alias(args[0], args[1], passwd, type)
	
class Alias:
	""" Aliases implementation
	"""
	def __init__(self, name, filename, passwd, type):
		""" Alias initialisation
		"""
		self.name = name
		self.filename = filename
		self.type = type
	
	def get(self, passwd):
		""" Get the list of files to send for this alias
		"""
		yield = []
		if self.type == ALIAS_TYPE_NORMAL:
			yield.append(self.filename)
			return yield
		elif self.type == ALIAS_TYPE_MAGIC:
			# Prepare the environment (TODO)
			putenv('PASSWORD', passwd)
			putenv('ADDRESS', None)
			putenv('PROTECTED', 'FALSE')
			putenv('LISTED', 'FALSE')
			# Execute magic program and process its output
			try:
				magic = popen(self.filename)
				line = magic.readline()
				while line:
					line = magic.readline()
					yield.append(string.strip(line))
				if magic.close():
					print "Magic return code is non-zero: ", self.filename
					return None
				return yield
			except IOError:
				print "Failed to run magic: ", self.filename
		return None	

class Config:
	def read_dir_list(self):
		yield = []
		fp = open(self.dir_list_file, 'r')
		line = fp.readline()
		while line:
			line = string.strip(line)
			if line != '':
				yield.append(line)
			line = fp.readline()
		fp.close()
		return yield
	
	def read_alias_list(self):
		yield = []
		fp = open(self.alias_list_file, 'r')
		line = fp.readline()
		while line:
			line = string.strip(line)
			if line != '':
				[name, filename] = string.split(line, None, 1)
				yield.append(alias(name, filename))
			line = fp.readline()
		fp.close()
		return yield
	
	def read(self, name):
		fp = open(name, 'r')
		line = fp.readline()
		while line:
			line = string.strip(line)
			args = string.split(line, None, 1)
			if line[0:1] == '#' or len(line) == 0:
				pass
			elif len(args) != 2:
				print "Invalid string in config: ", line
			else:
				key = string.lower(args[0])
				val = args[1]
				if key == 'dir-list-file':
					self.dir_list_file = val
				elif key == 'send-report':
					self.send_report = get_bool(val)
				elif key == 'limit-size-day':
					self.limit_size_day = get_size(val)
				elif key == 'limit-size-week':
					self.limit_size_week = get_size(val)
				elif key == 'limit-size-month':
					self.limit_size_month = get_size(val)
				elif key == 'spool-dir':
					self.spool_dir = val
				elif key == 'freq-alias':
					self.freq_alias.append(get_alias(val, ALIAS_TYPE_NORMAL))
				elif key == 'freq-magic':
					self.freq_magic.append(get_alias(val, ALIAS_TYPE_MAGIC))
				elif key == 'log-file':
					self.log_file = val
				elif key == 'local-address':
					self.local_address.parse(var)
				elif key == 'report-header':
					self.report_header = val
				elif key == 'report-footer':
					self.report_footer = val
				elif key == 'report-from':
					self.report_from = val
				elif key == 'report-subj':
					self.report_subj = val
				elif key == 'stat-dbase':
					self.stat_dbase = val
				else:
					print "unknown config keyword:", key
			line = fp.readline()
		fp.close()
		
	def __init__(self, name):
		self.dir_list_file = ''
		self.send_report = 0
		self.limit_size_day = 0
		self.limit_size_week = 0
		self.limit_size_month = 0
		self.spool_dir = ''
		self.freq_policy = ''
		self.freq_alias = []
		self.freq_magic = []
		self.log_file = ''
		self.local_address = ufido.address()
		self.report_header = ''
		self.report_footer = ''
		self.report_from = 'FREQ manager'
		self.report_subj = 'FREQ report'
		self.stat_dbase = None
		self.read(name)

