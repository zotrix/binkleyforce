#! /bin/sh
#
# skeleton	example file to build /etc/init.d/ scripts.
#		This file should be used to construct scripts for /etc/init.d.
#
#		Written by Miquel van Smoorenburg <miquels@cistron.nl>.
#		Modified for Debian 
#		by Ian Murdock <imurdock@gnu.ai.mit.edu>.
#
# Version:	@(#)skeleton  1.9  26-Feb-2001  miquels@cistron.nl
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.


PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/bin/bforce
NAME=bforce
DESC="fidonet mailer"
OWNER="uucp"
test -x $DAEMON || exit 0

# Include bforce defaults if available
if [ -f /etc/default/bforce/default ] ; then
	. /etc/default/bforce/default
fi

# We need to run bforce?
if [ $RUN == "no" ] ; then
    echo "binkleyforce startup is disabled in /etc/default/bforce"
    exit 0
fi

set -e

case "$1" in
  start)
	echo -n "Starting $DESC: "
	su $OWNER -c "$DAEMON -d"
	echo "$NAME."
	;;
  stop)
	echo -n "Stopping $DESC: "
	su $OWNER -c "$DAEMON -q"
	echo "$NAME."
	;;
  restart|force-reload)
	echo -n "Restarting $DESC: "
	su $OWNER "$DAEMON -q"
	sleep 1
	su $OWNER "$DAEMON -d"
	echo "$NAME."
	;;
  *)
	N=/etc/init.d/$NAME
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
