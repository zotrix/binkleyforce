import time
import gdbm
import ufido

class nodestat:
	""" [[month_id, month_size, month_num, month_time],
	[week_id, seek_size, week_num, week_time],
	[day_id, day_size, day_num, day_time],
	[total_size, total_num, total_time]]
	"""
	def __init__(self, dbpath, address):
		self.addr = address
		self.key = address.string()
		self.stat_session_size = 0
		self.stat_session_num = 0
		self.stat_session_time = 0
		self.stat_day_size = 0
		self.stat_day_num = 0
		self.stat_day_time = 0
		self.stat_week_size = 0
		self.stat_week_num = 0
		self.stat_week_time = 0
		self.stat_month_size = 0
		self.stat_month_num = 0
		self.stat_month_time = 0
		self.stat_total_size = 0
		self.stat_total_num = 0
		self.stat_total_time = 0
		self.dbpath = dbpath
		tt = time.localtime()
		self.month_id = time.strftime('%Y%m', tt)
		self.week_id = time.strftime('%Y%W', tt)
		self.day_id = time.strftime('%Y%j', tt)
		self.notexist = 0 # Entry for this node doesn't exist yet?
	
	def upd_stat(self, num, size):
		self.stat_session_size = self.stat_session_size + size
		self.stat_session_num = self.stat_session_num + num
		self.stat_month_size = self.stat_month_size + size
		self.stat_month_num = self.stat_month_num + num
		self.stat_week_size = self.stat_week_size + size
		self.stat_week_num = self.stat_week_num + num
		self.stat_day_size = self.stat_day_size + size
		self.stat_day_num = self.stat_day_num + num
		self.stat_total_size = self.stat_total_size + size
		self.stat_total_num = self.stat_total_num + num
	
	def get_stat(self):
		try:
			db = gdbm.open(self.dbpath, 'r')
		except gdbm.error:
			return 0
		if not db.has_key(self.key):
			self.notexist = 1
			db.close()
			return 0
		entry = eval(db[self.key])
		# Check month statistic
		if entry[0][0] == self.month_id:
			self.stat_month_size = entry[0][1]
			self.stat_month_num = entry[0][2]
			self.stat_month_time = entry[0][3]
		# Check week statistic
		if entry[1][0] == self.week_id:
			self.stat_week_size = entry[1][1]
			self.stat_week_num = entry[1][2]
			self.stat_week_time = entry[1][3]
		# Check day statistic
		if entry[2][0] == self.day_id:
			self.stat_day_size = entry[2][1]
			self.stat_day_num = entry[2][2]
			self.stat_day_time = entry[2][3]
		# Get total statistic
		self.stat_total_size = entry[3][0]
		self.stat_total_num = entry[3][1]
		self.stat_total_time = entry[3][2]
		db.close()
		return 0
	
	def put_stat(self):
		db = gdbm.open(self.dbpath, 'cf')
		# Don't handle exceptions
		entry = [[self.month_id, self.stat_month_size, self.stat_month_num, self.stat_month_time],
			[self.week_id, self.stat_week_size, self.stat_week_num, self.stat_week_time],
			[self.day_id, self.stat_day_size, self.stat_day_num, self.stat_day_time],
			[self.stat_total_size, self.stat_total_num, self.stat_total_time]]
		db[self.key] = repr(entry)
		db.close()
		return 0

if __name__ == '__main__':
	addr = ufido.address()
	addr.parse('2:5020/2120')
	ns = nodestat('./tmp.db', addr)
	ns.upd_stat(2, 32768)
	ns.put_stat()
	addr2 = ufido.address()
	addr2.parse('2:5020/2120')
	ns2 = nodestat('./tmp.db', addr2)
	ns2.get_stat()
	print ns2.stat_total_num, ns2.stat_total_size

