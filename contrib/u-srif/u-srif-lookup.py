#!/usr/local/bin/python

import sys

# Our own libraries
sys.path.append('./lib')
import uconfig
import udbase
from uutil import *

USRIF_CONFIG = '/usr/local/etc/u-srif/u-srif.conf'

##############################
# The main program starts here
if len(sys.argv) < 2:
	print 'usage:', sys.argv[0], '<[file] [file] ..>'
	sys.exit(1)

# Read configuration
Conf = uconfig.Config(USRIF_CONFIG)

# Lookup files in the database
FBase = udbase.filebase(Conf.spool_dir)
FBase.open('r')
yield = FBase.get_all(sys.argv[1:])
FBase.close()

# Pretty printing
print file_info_header
for file in yield:
	file.stat()
	print format_file_info(file.name, file.size, file.desc)

sys.exit(0)

