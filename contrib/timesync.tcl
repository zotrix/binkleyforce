#!/usr/bin/tclsh
#
# Copyright (c) 2000 by Alexander Belkin <adb@newmail.ru>
#
# $Id$
#
# Time syncronization utility
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

##################
# Program settings

set bforce_log_file "/var/log/bforce/bf-log.ttyS0"
set sync_with_addr "2:450/102"
set max_time_diff 1800
set min_time_diff 20
set news_groups "local.robots"
set from_user "Time Robot <root@fido.xxx.local>"
set inews_cmd "/usr/bin/inews"

################################
# The program's body starts here

set sess_pid ""
set remote_times ""
set local_times ""
set TEXT ""

set INP [open $bforce_log_file "r"]
foreach line [split [read $INP] "\n"] {

	set fields [split $line " "]
	set pid [lindex $fields 3]
	set time [join [lrange $fields 0 2]]
	set data [join [lrange $fields 4 end]]

	if { [string match "*Address : $sync_with_addr" $data] } {
		set sess_pid $pid
		set sess_time ""
	} elseif { [string match "*Time :*" $data] } {
		if { $sess_pid == $pid } {
			regexp "Time : (.+)" $data {} timestr
			if { $timestr != "" } {
				set sess_time $timestr
			}
		}
	} elseif { [string match "remote is*,protected" $data] } {
		if { $sess_pid == $pid } {
			if { $sess_time != "" && $time != "" } {
				lappend remote_times $sess_time
				lappend local_times  $time
			}
			set sess_pid  ""
			set sess_time ""
		}
	} elseif { [string match "remote is*" $data] } {
		if { $sess_pid == $pid } {
			set sess_pid  ""
			set sess_time ""
		}
	}
		
}
close $INP

set last_rem_time [lindex $remote_times end]
set last_loc_time [lindex $local_times end]

if { $last_rem_time != "" && $last_loc_time != "" } {

	regexp "(\[0-9]+):(\[0-9]+):(\[0-9]+)" $last_rem_time {} rh rm rs
	regexp "(\[0-9]+):(\[0-9]+):(\[0-9]+)" $last_loc_time {} lh lm ls
	set rem [expr "$rh*3600 + $rm*60 + $rs"]
	set loc [expr "$lh*3600 + $lm*60 + $ls"]
	set diff [expr "$rem - $loc"]
	
	if { [expr abs($diff)] < $min_time_diff } {
		# Do nothing
	} elseif { [expr abs($diff)] <= $max_time_diff } {
		set old_sec [clock seconds]
		set new_sec [expr $old_sec + $diff]
		
		exec /bin/date -s [clock format $new_sec -format "%d-%b-%Y %H:%M:%S"]
		exec /sbin/clock -wu
		
		append TEXT "The System time was synchronized with the node $sync_with_addr\n\n"
		append TEXT "Old time        : [clock format $old_sec]\n"
		append TEXT "New time        : [clock format $new_sec]\n"
		append TEXT "Time difference : $diff second(s)\n"
	} else {
		set old_sec [clock seconds]
		set new_sec [expr $old_sec + $diff]
		
		append TEXT "The System time WAS NOT synchronized with the node $sync_with_addr\n\n"
		append TEXT "Current time    : [clock format $old_sec]\n"
		append TEXT "Want set time   : [clock format $new_sec]\n"
		append TEXT "Time difference : $diff second(s) (must be lower $max_time_diff seconds)\n"
	}
}

if { $TEXT != "" } {
	
	set MSG "From: $from_user\n"
	append MSG "Subject: Time synchronization\n"
	append MSG "Newsgroups: $news_groups\n\n"
	append MSG $TEXT

	exec $inews_cmd -h << $MSG
}

exit 0

