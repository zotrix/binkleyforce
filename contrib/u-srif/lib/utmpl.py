import string

class template:
	def __init__(self):
		self.local_address = ''
		self.local_sysop = ''
		self.local_location = ''
		self.local_phone = ''
		self.remote_address = ''
		self.remote_sysop = ''
		self.remote_location = ''
		self.remote_phone = ''
		self.remote_cid = ''
		self.limit_size_day = -1
		self.limit_size_week = -1
		self.limit_size_month = -1
		self.sent_size_day = -1
		self.sent_size_week = -1
		self.sent_size_month = -1
		self.sent_size = -1
		self.conn_speed = -1
		self.text = None
	
	def set(self, srif=None, conf=None, nodestat=None):
		if srif:
			self.remote_address = srif.aka.string()
			self.remote_sysop = srif.sysop
			self.remote_location = srif.site
			self.remote_cid = srif.callerid
			self.conn_speed = srif.baud
		if conf:
			self.local_address = conf.local_address.string()
			self.limit_size_day = conf.limit_size_day
			self.limit_size_week = conf.limit_size_week
			self.limit_size_month = conf.limit_size_month
		if nodestat:
			self.sent_session_size = nodestat.stat_session_size
			self.sent_session_num = nodestat.stat_session_num
			self.sent_day_size = nodestat.stat_day_size
			self.sent_day_num = nodestat.stat_day_num
			self.sent_week_size = nodestat.stat_week_size
			self.sent_week_num = nodestat.stat_week_num
			self.sent_month_size = nodestat.stat_month_size
			self.sent_month_num = nodestat.stat_month_num
			self.sent_total_size = nodestat.stat_total_size
			self.sent_total_num = nodestat.stat_total_num
	
	def __cmd__(self, str):
		try:
			[fmt, arg] = string.split(str, ',', 1)
			return eval('"' + fmt + '" % self.' + arg)
		except ValueError:
			return '@ValueError@'
		except AttributeError:
			return '@AttributeError@'
	
	def process(self, text=None):
		if text == None:
			text = self.text
		if text == None:
			return None
		pos = 0
		while 1:
			pos = string.find(text, '@', pos)
			if pos < 0:
				break
			end_pos = string.find(text, '@', pos+1)
			if end_pos < 0:
				break
			if end_pos - pos > 1:
				# Process escaped command
				replace = self.__cmd__(text[pos+1:end_pos])
				if replace:
					text = text[:pos]+replace+text[end_pos+1:]
					# Fix the current position
					pos = end_pos + len(replace)-(end_pos-pos+1)
				else:
					# Leave text untouched
					pos = end_pos + 1
			else:
				# Replace '@@' by the single '@'
				text = text[:pos+1]+text[pos+2:]
				pos = end_pos
		return text
	
	def readfile(self, path):
		try:
			fp = open(path, 'r')
			self.text = fp.read()
			fp.close()
		except IOError:
			pass

if __name__ == "__main__":
	test = template()
	print test.process("'@@'\n'@@'\n'@%d,conn_speed@'\n'@@@'")

