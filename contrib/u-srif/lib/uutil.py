
import string

# Header for the files information
file_info_header = 'File                 Size        Description\n'\
                 + '-' * 78

class ULog:
	def __init__(self, path):
		self.path = path
		self.fp = open(path, 'a')
	
	def puts(self, string):
		fp.puts(strftime('%b %d %H:%M:%S ', gmtime())+string)
	
	def close(self):
		fp.close()

def format_desc(desc, offset, width=78):
	""" Format file's description
	"""
	yield = ''
	desc = string.expandtabs(desc, 1)
	for line in string.split(desc, '\n'):
		line = string.rstrip(line)
		if line == '':
			continue
		pos = 0
		endpos = width
		while line[pos:endpos]:
			if yield:
				yield = yield + '\n'
			yield = yield + offset * ' '
			yield = yield + line[pos:endpos]
			pos = endpos
			endpos = endpos + width
	return yield

def format_file_info(name, size, desc, line_length=78):
	""" Format file information to meet human requirements
	"""
	if size < 0:
		yield = '%-20s ' % name + 11 * ' '
	else:
		yield = '%-20s %-11d' % (name, size)
	if desc:
		offset = len(yield) + 1
		width = line_length - offset
		desc = format_desc(desc, offset, width)
		yield = yield + ' ' + string.lstrip(desc)
	else:
		yield = yield + ' Description not available'
	return yield

