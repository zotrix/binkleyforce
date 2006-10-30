#! /bin/sh
#
# Version:	@(#)bforce  1.1  30-Oct-2006  e.kozhuhovskiy@gmail.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.


PATH=/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/bin/bforce
NAME=bforce
DESC="fidonet mailer"
OWNER="news"
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
	su -c "$DAEMON -d" $OWNER
	echo "$NAME."
	;;
  stop)
	echo -n "Stopping $DESC: "
	su -c "$DAEMON -q" $OWNER
	echo "$NAME."
	;;
  restart|force-reload)
	echo -n "Restarting $DESC: "
	su -c "$DAEMON -q" $OWNER
	sleep 1
	su -c "$DAEMON -d" $OWNER
	echo "$NAME."
	;;
  *)
	N=/etc/init.d/$NAME
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
