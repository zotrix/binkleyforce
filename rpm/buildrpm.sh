#!/bin/sh

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

RPMROOT='/home/ugenk/rpm'

if [ `whoami` != "root" ]; then
    echo "$0: your must be root run this script."
    exit 1
fi


cd ../source

make distclean

cd -

rm -f *.gz

tar -cvf bforce-`cat ../source/.version`.tar ../../bforce-`cat ../source/.version`/source/ ../../bforce-`cat ../source/.version`/CHANGES* ../../bforce-`cat ../source/.version`/COPYING ../../bforce-`cat ../source/.version`/INSTALL* ../../bforce-`cat ../source/.version`/README* ../../bforce-`cat ../source/.version`/SYSLOG ../../bforce-`cat ../source/.version`/TODO  ../../bforce-`cat ../source/.version`/contrib ../../bforce-`cat ../source/.version`/examples ../../bforce-`cat ../source/.version`/debian

gzip bforce-`cat ../source/.version`.tar

cp -f bforce-`cat ../source/.version`.tar.gz $RPMROOT/SOURCES/

rpm -ba bforce.spec
 
