
import os
import gdbm
import string

def get_file_desc(filename):
	path, name = os.path.split(filename)
	descname = os.path.join(path, '.desc', name + '.desc')
	if not os.path.isfile(descname):
		return None
	try:
		fp = open(descname, 'r')
	except IOError:
		print 'Cannot open', descname
		return None
	line = fp.readline()
	yield = ''
	while line:
		yield = yield + line
		line = fp.readline()
	fp.close()
	return string.strip(yield)

def get_area_desc(areapath):
	descname = os.path.join(areapath, '.desc', '.desc')
	if not os.path.isfile(descname):
		return None
	try:
		fp = open(descname, 'r')
	except IOError:
		print 'Cannot open', descname
		return None
	line = fp.readline()
	yield = ''
	while line:
		yield = yield + line
		line = fp.readline()
	fp.close()
	return string.strip(yield)

class file:
	def stat(self):
		if self.fake:
			return
		try:
			statinfo = os.stat(self.fullname)
			self.size = statinfo[6]
			self.time = statinfo[8]
		except OSError:
			self.size = -1
			self.time = -1
			self.desc = 'File is not accessable'
	
	def set(self, fullname, name = None, area = '', dlcnt = 0,\
	              mode = '', desc = '', fake = 0):
		if name == None:
			self.name = os.path.split(fullname)[1]
		else:
			self.name = name
		self.fullname = fullname
		self.dlcnt = dlcnt
		self.size = -1
		self.time = -1
		self.area = area
		self.mode = mode
		self.desc = desc
		self.fake = fake
	
	def reset(self):
		self.name = ''
		self.fullname = ''
		self.area = ''
		self.dlcnt = 0
		self.mode = ''
		self.desc = ''
		self.size = -1
		self.time = -1
		self.fake = 0
	
	def __init__(self):
		self.reset()

class filebase:
	""" Index entry format: [fullname, area, dlcnt, accessmode, desc]
	"""
	def open(self, mode):
		self.db = gdbm.open(self.dbfile, mode)
	
	def close(self):
		self.db.close()
	
	def sync(self):
		self.db.sync()
	
	def clean(self):
		for filename in self.db.keys():
			file = self.get(filename)
			if not os.path.isfile(file.fullname):
				print 'Remove file "%s" from index' % file.fullname
				del self.db[filename]
	
	def get_all(self, filenames):
		""" Lookup files in the database and return list
		of file objects
		"""
		yield = []
		for name in filenames:
			files = self.get(name)
			if files and len(files) > 0:
				yield.extend(files)
		return yield

	def get(self, filename):
		""" Lookup file by its name in the database and
		return list of file objects for this name
		"""
		yield = []
		if not self.db.has_key(filename):
			return None
		files_info = eval(self.db[filename])
		if files_info == None:
			newfile = file()
			newfile.set(filename, desc='File not found', fake=1)
			yield.append(newfile)
		else:
			for finfo in files_info:
				newfile = file()
				finfo = eval(finfo)
				newfile.set(finfo[0], area = finfo[1],\
					dlcnt = finfo[2], mode = finfo[3],\
					desc = finfo[4])
				yield.append(newfile)
		return yield
	
	def put(self, file):
		finfo = []
		finfo.append(file.fullname)
		finfo.append(file.area)
		finfo.append(file.dlcnt)
		finfo.append(file.mode)
		finfo.append(file.desc)
		if self.db.has_key(file.name):
			files_info = eval(self.db[file.name])
		else:
			files_info = []
		files_info.append(repr(finfo))
		self.db[file.name] = repr(files_info)
	
	def __init__(self, spooldir):
		self.dbfile = os.path.join(spooldir, 'filebase.db')

