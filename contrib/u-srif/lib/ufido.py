
import string
import re
import struct
import time

#address_1 = re.compile('^\([0-9]+\):\([0-9]+\)/\([0-9]+\)\.?\([0-9]+\)?$')
address_1 = re.compile('^(\d+):(\d+)/(\d+)\.?(\d+)?$')

months = ('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',\
          'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Jan')

class address:
	def is_set(self):
		if self.zone > 0 and self.net > 0:
			return 1
		return 0
	
	def string(self):
		if self.invalid:
			yield = 'Invalid address'
		elif self.point > 0:
			yield = '%d:%d/%d.%d' % (self.zone, self.net, self.node, self.point)
		else:
			yield = '%d:%d/%d' % (self.zone, self.net, self.node)
		return yield
	
	def parse(self, str):
		match = address_1.match(str)
		if match:
			try:
				self.zone = string.atoi(match.group(1))
				self.net = string.atoi(match.group(2))
				self.node = string.atoi(match.group(3))
				tmp = match.group(4)
				if tmp and tmp != '-1':
					self.point = string.atoi(tmp)
				else:
					self.point = 0
				self.invalid = 0
			except IndexError:
				self.__init__()
				return -1
		else:
			print "Regexp doesnt match!"
			return -1
		return 0

	def __init__(self):
		self.zone = 0
		self.net = 0
		self.node = 0
		self.point = 0
		self.invalid = 1

class message:
	def newmsg(self, addr_from, user_from, addr_to, user_to, subject):
		self.unix_time = time.time()
		self.addr_orig = addr_from
		self.addr_dest = addr_to
		self.user_orig = user_from
		self.user_dest = user_to
		self.subject = subject
		self.msgbody = ''
		self.append_line('\001FMPT %d' % self.addr_orig.point)
		self.append_line('\001TOPT %d' % self.addr_dest.point)
	
	def append_line(self, string):
		self.msgbody = self.msgbody + string + '\r'
	
	def append_text(self, text):
		for line in string.split(text, '\n'):
			self.append_line(line)
	
	def append_file(self, filename):
		try:
			fp = open(filename, 'r')
		except IOError:
			print 'Cannot append file', filename
			return
		line = fp.readline()
		while line:
			self.append_line(string.rstrip(line))
			line = fp.readline()
		fp.close()
	
	def __init__(self):
		self.unix_time = 0
		self.addr_orig = address()
		self.addr_dest = address()
		self.user_orig = ''
		self.user_dest = ''
		self.subject = ''
		self.msgbody = ''
		self.origin = ''

class packet:
	def reset(self):
		self.addr_orig = address()
		self.addr_dest = address()
		self.password = ''
		self.messages = []
	
	# TODO
	def read(self, filename):
		self.reset()
		fp = open(filename, "w")
		fp.close()
	
	def get_time_string(self, unix_time):
		msgtime = time.localtime(unix_time)
		return '%02d %s %02d   %02d:%02d:%02d' % \
			(msgtime[2], months[msgtime[1]], msgtime[0] % 100, \
			 msgtime[3], msgtime[4], msgtime[5])
	
	def get_message_header(self, message):
		return struct.pack('7H20s',\
			2,\
			message.addr_orig.node,\
			message.addr_dest.node,\
			message.addr_orig.net,\
			message.addr_dest.net,\
			0,\
			0,\
			self.get_time_string(message.unix_time)) + \
			message.user_dest[0:36] + '\0' + \
			message.user_orig[0:36] + '\0' + \
			message.subject[0:72] + '\0'
	
	def get_packet_header(self):
		now = time.localtime(time.time())
		return struct.pack('13H8s12H',\
			self.addr_orig.node,\
			self.addr_dest.node,\
			now[0], # Year\
			now[1], # Month\
			now[2], # Day\
			now[3], # Hour\
			now[4], # Minute\
			now[5], # Second\
			9600,   # Baud\
			2,      # PKT type\
			self.addr_orig.net,\
			self.addr_dest.net,\
			254,    # Prod. code + Rev. number\
			self.password,\
			self.addr_orig.zone,\
			self.addr_dest.zone,\
			0,      # AuxNet\
			0,      # CWvalidationCopy\
			0,      # ProductCode + Revision \
			0,      # CapabilWord\
			self.addr_orig.zone,\
			self.addr_dest.zone,\
			self.addr_orig.point,\
			self.addr_dest.point,\
			0,
			0)
	
	def write(self, filename):
		fp = open(filename, "w")
		fp.write(self.get_packet_header())
		
		for msg in self.messages:
			fp.write(self.get_message_header(msg))
			fp.write(msg.msgbody)
			fp.write('\0')
		
		fp.write('\0\0')
		fp.close()
	
	def add_message(self, message):
		self.messages.append(message)
	
	def __init__(self):
		self.reset()

if __name__ == "__main__":
	tmp = address()
	tmp.parse('2:5020/2120')
	print tmp.string()
