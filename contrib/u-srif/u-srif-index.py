#!/usr/bin/python

import sys
import posix
import os
import libconf
import libfbase

USRIF_CONFIG = '/usr/local/etc/u-srif/u-srif.conf'

##############################
# The main program starts here

# Read configuration
Conf = libconf.config(USRIF_CONFIG)

# Open files index for writing
FBase = libfbase.filebase(Conf.spool_dir)
FBase.open('cwf')

file = libfbase.file()

# Process aliases from 'alias-list-file'
for alias in Conf.read_alias_list():
	print 'Processing alias "%s": %s' % (alias.name, alias.filename)
	file_desc = libfbase.get_file_desc(alias.filename)
	file.set(alias.filename, name = alias.name, desc = file_desc)
	file.stat()
	FBase.put(file)

# Process dirs from 'dir-list-file'
for dir in Conf.read_dir_list():
	area_desc = libfbase.get_area_desc(dir)
	print 'Processing: %s (%s)' % (dir, area_desc)
	files_list = posix.listdir(dir)
	for file_name in files_list:
		full_name = os.path.join(dir, file_name)
		if os.path.isfile(full_name):
			file_desc = libfbase.get_file_desc(full_name)
			file.set(full_name, area = area_desc, desc = file_desc)
			file.stat()
			FBase.put(file)

# Purge files index
#print 'Purging removed files from files index'
#FBase.clean()

FBase.close()
sys.exit(0)

