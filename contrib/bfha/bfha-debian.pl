#!/usr/bin/perl

#
# bfha -- binkleyforce history analyzer
#
# Copyright (C) 2000 Serge N. Pokhodyaev
#
# E-mail: snp@ru.ru
# Fido:   2:5020/1838
#
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
#
# $Id$
#

#
# Á≈Œ≈“…‘ ”‘¡‘…”‘…À’ ⁄¡ –“≈ƒŸƒ’›…  ƒ≈Œÿ
#

#
# TODO:
#
# 1. ıﬁ≈”‘ÿ ”Ã’ﬁ¡ , Àœ«ƒ¡ Ãœ« –œ◊£“Œ’‘ logrotate'œÕ
#
# Known bugs:
#
# 1. H≈–“¡◊…ÃÿŒœ ”ﬁ…‘¡≈‘ cps (⁄¡◊Ÿ€¡≈‘) ≈”Ã… ‘“¡∆…À ◊ œ¬≈ ”‘œ“œŒŸ
#

use strict;
use Time::Local;
use POSIX qw(strftime);

my $PROGNAME = 'bfha';
my $VERSION = '$Revision$ ';

######## Configurable part ###################################################

#my $inews = "/usr/bin/inews -h -O -S";

#my $log = "/var/log/fido/history";

#my $rep_newsgroups = "ftn.1838.stat";
#my $rep_from = "\"Statistics generator\" <snp\@gloom.intra.eu.org>";
#my $rep1_subj = "Sessions history";
#my $rep2_subj = "Sessions history";
#my $rep3_subj = "Links statistics";

my $count_failed = 1;

#my %line_names = (
#		  "ttyS1" => "Modem",
#		  "tcpip" => "IP"
#);

##############################################################################

my $devel = 0;

my (%st, %lines, %nodes, @nodes_sorted);
my @tm;
my $time;

die "Usage: $PROGNAME [-d]\n" if (!((0 == $#ARGV) && ($ARGV[0] eq '-d')) && !(-1 == $#ARGV));
$devel = 1 if ((0 == $#ARGV) && ($ARGV[0] eq '-d'));

$log = "./history" if ($devel);

# Make version string
#
$VERSION =~ s/(\$Rev)ision:\s([\d.]+).*/$PROGNAME\/$2/;

# Get time of 0:00:00 of yesterday
#
@tm = localtime(time - 86400);
$tm[0] = 0;	# sec
$tm[1] = 0;	# min
$tm[2] = 0;	# hours
$time = timelocal(@tm);

# At first read logs
#
undef %st;
undef %lines;
undef %nodes;
die "Log reading error\n" if (0 != log_read($time));

@nodes_sorted = sort(node_cmp keys(%nodes));

# Now generate statistics
#
out_close();
#rep1();
rep2();
rep3();


sub log_read
  {
    my $i;
    my $t;
    my @l;

    die if (0 != $#_);

    $t = $_[0];

    open(LOG, "< $log") || die("Can't open $log: $!\n");
    while (<LOG>)
      {
	chomp;
	@l = split(/,/);
	return 1 if ($#l != 11);
	return 1 if (! ($l[4] =~ /[IO]/));

	# Check date
	#
	next if ($l[2] < $t);
	last if ($l[2] >= ($t + 86400));

	# Entries without address
	#
	if ($l[1] eq '')
	  {
	    next if (0 == $count_failed);
	    $l[1] = 'failed' if (0 != $count_failed);
	  }

	# Remove domain
	#
	$l[1] =~ s/@.*$//;

	# Add statistics
	#
	$i = $#{$st{beg}} + 1;

	$lines{$l[0]}[$#{$lines{$l[0]}} + 1] = $i;
	$nodes{$l[1]}[$#{$nodes{$l[1]}} + 1] = $i;

	$st{beg}[$i] = $l[2];
	$st{len}[$i] = $l[3];
	$st{addr}[$i] = $l[1];
	$st{line}[$i] = $l[0];
	$st{rc}[$i] = $l[5];
	$st{snt_nm}[$i] = $l[6];
	$st{snt_am}[$i] = $l[7];
	$st{snt_f}[$i] = $l[8];
	$st{rcv_nm}[$i] = $l[9];
	$st{rcv_am}[$i] = $l[10];
	$st{rcv_f}[$i] = $l[11];
	$st{islst}[$i] = 0;
	$st{islst}[$i] = 1 if ($l[4] =~ /L/);
	$st{isprot}[$i] = 0;
	$st{isprot}[$i] = 1 if ($l[4] =~ /P/);
	$st{type}[$i] = 'I' if ($l[4] =~ /I/);
	$st{type}[$i] = 'O' if ($l[4] =~ /O/);
      }
  }

######## rep1 ################################################################

sub rep1
  {
    my ($i, $j);
    my (%se, %se_t);
    my $node;

    out_open($rep1_subj);
    printf "Date: %s\n\n", strftime("%a, %e %b %Y", localtime($time));

    $~ = "rep1_header";
    write;

    $~ = "rep1_body";
    $se_t{num_in} = 0;
    $se_t{num_out} = 0;
    $se_t{snt} = 0;
    $se_t{rcv} = 0;
    $se_t{len} = 0;
    foreach $node (@nodes_sorted)
      {
	$se{num_in} = 0;
	$se{num_out} = 0;
	$se{snt} = 0;
	$se{rcv} = 0;
	$se{len} = 0;
	for ($i = 0; $i <= $#{$nodes{$node}}; ++$i)
	  {
	    $j = $nodes{$node}[$i];
	    ++$se{num_in} if ($st{type}[$j] eq 'I');
	    ++$se{num_out} if ($st{type}[$j] eq 'O');
	    $se{snt} += $st{snt_am}[$j] + $st{snt_nm}[$j] + $st{snt_f}[$j];
	    $se{rcv} += $st{rcv_am}[$j] + $st{rcv_nm}[$j] + $st{rcv_f}[$j];
	    $se{len} += $st{len}[$j];
	  }
	$se{time} = time_int2str($se{len});
	# FIXME (cps)
	$se{cps} = div_int(($se{snt} + $se{rcv}), $se{len});
	write;

	$se_t{num_in} += $se{num_in};
	$se_t{num_out} += $se{num_out};
	$se_t{snt} += $se{snt};
	$se_t{rcv} += $se{rcv};
	$se_t{len} += $se{len};
      }

    $~ = "rep1_footer";
    $se_t{time} = time_int2str($se_t{len});
    # FIXME (cps)
    $se_t{cps} = div_int(($se_t{snt} + $se_t{rcv}), $se_t{len});
    write;

    print "\n";
    out_close();


format rep1_header =
•†††††††††††††††††∂††††††††††∂†††††††††††∂††††††††††††∂†††††††††††∂†††††††®
°     System      Å Sessions Å   Sent    Å  Received  Å   Time    Å  CPS  °
°     address     Å in   out Å   bytes   Å   bytes    Å  online   Å       °
±†††††††††††††††††º††††††††††º†††††††††††º††††††††††††º†††††††††††º†††††††µ
.

format rep1_body =
° @<<<<<<<<<<<<<< Å @>>  @>> Å @>>>>>>>> Å @>>>>>>>>> Å @>>>>>>>> Å @>>>> °
$node, $se{num_in}, $se{num_out}, $se{snt}, $se{rcv}, $se{time}, $se{cps}
.

format rep1_footer =
∞ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄäÄÄÄÄÄÄÄÄÄÄäÄÄÄÄÄÄÄÄÄÄÄäÄÄÄÄÄÄÄÄÄÄÄÄäÄÄÄÄÄÄÄÄÄÄÄäÄÄÄÄÄÄÄ¥
°      TOTAL      Å @>>  @>> Å @>>>>>>>> Å @>>>>>>>>> Å @>>>>>>>> Å @>>>> °
$se_t{num_in}, $se_t{num_out}, $se_t{snt}, $se_t{rcv}, $se_t{time}, $se_t{cps}
´†††††††††††††††††π††††††††††π†††††††††††π††††††††††††π†††††††††††π†††††††Æ
.
  }

######## rep2 ################################################################

sub rep2
  {
    my ($i, $j);
    my @t;
    my ($t1, $t2, $str, $lv);
    my $node;
    my %se;

    out_open($rep2_subj);
    printf "Date: %s\n\n", strftime("%a, %e %b %Y", localtime($time));

    $~ = "rep2_header";
    write;

    $~ = "rep2_body";
    foreach $node (@nodes_sorted)
      {
	$se{snt} = 0;
	$se{rcv} = 0;
	$se{len} = 0;
	for ($i = 0; $i <= 95; ++$i)
	  {
	    $t[$i] = 0;
	  }
	for ($i = 0; $i <= $#{$nodes{$node}}; ++$i)
	  {
	    $j = $nodes{$node}[$i];

	    # Fill array
	    #
	    $t1 = $st{beg}[$j] - $time;
	    $t2 = $t1 + $st{len}[$j];
	    $t2 = 86399 if ($t2 > 86399);
	    $t1 = div_int($t1, 900);
	    $t2 = div_int($t2, 900);
	    while ($t1 <= $t2)
	      {
		$t[$t1++] = 1;
	      }

	    $se{snt} += $st{snt_am}[$j] + $st{snt_nm}[$j] + $st{snt_f}[$j];
	    $se{rcv} += $st{rcv_am}[$j] + $st{rcv_nm}[$j] + $st{rcv_f}[$j];
	    $se{len} += $st{len}[$j];
	  }
	$se{time} = time_int2str($se{len});
	# FIXME (cps)
	$se{cps} = div_int(($se{snt} + $se{rcv}), $se{len});

	# Visualize
	#
	$i = 0;
	$str = "";
	if ($t[$i++])
	  {
	    $str = $str . "è";
	  }
	else
	  {
	    $str = $str . "Å";
	  }
	while ($i < $#t)
	  {
	    $lv = 0;
	    $lv += 1 if ($t[$i++]);
	    $lv += 2 if ($t[$i++]);

	    if (0 == $lv)
	      {
		if (div_rest(($i - 1), 8) == 0)
		  {
		    $str = $str . "Å";
		  }
		else
		  {
		    $str = $str . " ";
		  }
	      }
	    elsif (1 == $lv)
	      {
		$str = $str . "é";
	      }
	    elsif (2 == $lv)
	      {
		$str = $str . "è";
	      }
	    elsif (3 == $lv)
	      {
		$str = $str . "ç";
	      }
	  }
	if ($t[$i])
	  {
	    $str = $str . "é";
	  }
	else
	  {
	    $str = $str . "Å";
	  }

	write;
      }

    $~ = "rep2_footer";
    write;

    out_close();


format rep2_header =
               0   2   4   6   8  10  12  14  16  18  20  22  24
               ÜÄâÄäÄâÄäÄâÄäÄâÄäÄâÄäÄâÄäÄâÄäÄâÄäÄâÄäÄâÄäÄâÄäÄâÄá
.

format rep2_body =
@<<<<<<<<<<<<<<@||||||||||||||||||||||||||||||||||||||||||||||||
$node, $str
.

format rep2_footer =
               ÜÄàÄäÄàÄäÄàÄäÄàÄäÄàÄäÄàÄäÄàÄäÄàÄäÄàÄäÄàÄäÄàÄäÄàÄá
               0   2   4   6   8  10  12  14  16  18  20  22  24
.
  }

######## rep3 ################################################################

sub rep3
  {
    my $node;
    my (%se, %se_t);
    my ($i, $j);

    out_open($rep3_subj);
    printf "Date: %s\n\n", strftime("%a, %e %b %Y", localtime($time));

    $~ = "rep3_header";
    write;

    $~ = "rep3_body";
    $se_t{num_in} = 0;
    $se_t{num_out} = 0;
    $se_t{time_in} = 0;
    $se_t{time_out} = 0;
    $se_t{snt} = 0;
    $se_t{rcv} = 0;
    $se_t{time} = 0;
    foreach $node (@nodes_sorted)
      {
	$se{num_in} = 0;
	$se{num_out} = 0;
	$se{time_in} = 0;
	$se{time_out} = 0;
	$se{snt} = 0;
	$se{rcv} = 0;
	$se{time} = 0;
	for ($i = 0; $i <= $#{$nodes{$node}}; ++$i)
	  {
	    $j = $nodes{$node}[$i];
	    ++$se{num_in} if ($st{type}[$j] eq 'I');
	    ++$se{num_out} if ($st{type}[$j] eq 'O');
	    $se{time_in} += $st{len}[$j] if ($st{type}[$j] eq 'I');
	    $se{time_out} += $st{len}[$j] if ($st{type}[$j] eq 'O');
	    $se{snt} += $st{snt_am}[$j] + $st{snt_nm}[$j] + $st{snt_f}[$j];
	    $se{rcv} += $st{rcv_am}[$j] + $st{rcv_nm}[$j] + $st{rcv_f}[$j];
	    $se{time} += $st{len}[$j];
	  }

	# Total counters
	#
	$se_t{num_in} += $se{num_in};
	$se_t{num_out} += $se{num_out};
	$se_t{snt} += $se{snt};
	$se_t{rcv} += $se{rcv};
	$se_t{time} += $se{time};
	$se_t{time_out} += $se{time_out};
	$se_t{time_in} += $se{time_in};

	# Output string
	#
	# FIXME (cps)
	$se{cps} = dash_if_zero(div_int(($se{snt} + $se{rcv}), $se{time}));
	$se{time} = time_int2str($se{time});
	$se{time_in} = time_int2str($se{time_in});
	$se{time_out} = time_int2str($se{time_out});
	$se{num_in} = dash_if_zero($se{num_in});
	$se{num_out} = dash_if_zero($se{num_out});
	$se{rcv} = shrink_size(dash_if_zero($se{rcv}));
	$se{snt} = shrink_size(dash_if_zero($se{snt}));
	write;
      }

    $~ = "rep3_footer";
    # FIXME (cps)
    $se_t{cps} = dash_if_zero(div_int(($se_t{snt} + $se_t{rcv}), $se_t{time}));
    $se_t{time} = time_int2str($se_t{time});
    $se_t{time_in} = time_int2str($se_t{time_in});
    $se_t{time_out} = time_int2str($se_t{time_out});
    $se_t{num_in} = dash_if_zero($se_t{num_in});
    $se_t{num_out} = dash_if_zero($se_t{num_out});
    $se_t{rcv} = shrink_size(dash_if_zero($se_t{rcv}));
    $se_t{snt} = shrink_size(dash_if_zero($se_t{snt}));
    $~ = "rep3_footer";
    write;

    print "\n";
    out_close();


format rep3_header =
•††††††††††††††††∏†††††††††††††††††††††††††††∏††††††††††∏†††††††††††††∏†††††††®
°                °      Sessions/Online      °   Time   °   Traffic   °       °
°     Address    ±†††††††††††††∏†††††††††††††µ  online  ±††††††∏††††††µ  CPS  °
°                °   Incoming  °   Outgoing  °          ° Rcvd ° Sent °       °
±††††††††††††††††æ†††∂†††††††††æ†††∂†††††††††æ††††††††††æ††††††æ††††††æ†††††††µ
.

format rep3_body =
° @<<<<<<<<<<<<<<°@>>Å@>>>>>>> °@>>Å@>>>>>>> °@>>>>>>>> °@>>>>>°@>>>>>°@>>>>> °
$node, $se{num_in}, $se{time_in}, $se{num_out}, $se{time_out}, $se{time}, $se{rcv}, $se{snt}, $se{cps}
.

format rep3_footer =
∞ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄΩÄÄÄäÄÄÄÄÄÄÄÄÄΩÄÄÄäÄÄÄÄÄÄÄÄÄΩÄÄÄÄÄÄÄÄÄÄΩÄÄÄÄÄÄΩÄÄÄÄÄÄΩÄÄÄÄÄÄÄ¥
° TOTAL          °@>>Å@>>>>>>> °@>>Å@>>>>>>> °@>>>>>>>> °@>>>>>°@>>>>>°@>>>>> °
$se_t{num_in}, $se_t{time_in}, $se_t{num_out}, $se_t{time_out}, $se_t{time}, $se_t{rcv}, $se_t{snt}, $se_t{cps}
´††††††††††††††††ª†††π†††††††††ª†††π†††††††††ª††††††††††ª††††††ª††††††ª†††††††Æ
.
  }

##############################################################################

sub shrink_size
  {
    die if (0 != $#_);

    return $_[0] if ($_[0] < 1024);
    return sprintf("%.1fk", $_[0] / 1024) if ($_[0] < 1048576);
    return sprintf("%.1fM", $_[0] / 1048576);
  }

sub dash_if_zero
  {
    die if (0 != $#_);

    return "-" if (0 == $_[0]);
    return $_[0];
  }

sub time_int2str
  {
    my $time;
    my $h;
    my $m;
    my $s;

    die if (0 != $#_);
    die if (86399 < $time);

    return "-:--:--" if (0 == $_[0]);

    $time = $_[0];
    $h = div_int($time, 3600);
    $time = div_rest($time, 3600);
    $m = div_int($time, 60);
    $s = div_rest($time, 60);

    return sprintf("%d:%2.2d:%2.2d", $h, $m, $s);
  }

# „≈Ãœﬁ…”Ã≈ŒŒœ≈ ƒ≈Ã≈Œ…≈
sub div_int
  {
    use integer;

    die if (1 != $#_);

    return 0 if ($_[1] == 0);
    return $_[0] / $_[1];
  }


# Ô”‘¡‘œÀ œ‘ √≈Ãœﬁ…”Ã≈ŒŒœ«œ ƒ≈Ã≈Œ…—
sub div_rest
  {
    die if (1 != $#_);

    return 0 if ($_[1] == 0);
    return $_[0] - (div_int($_[0], $_[1]) * $_[1]);
  }

sub out_open
  {
    die if (0 != $#_);

    open(STDOUT, "| $inews") || die("Can't pipe to inews: $!\n") if (! $devel);
    printf "Newsgroups: %s\n", $rep_newsgroups;
    printf "From: %s\n", $rep_from;
    printf "Subject: %s\n", $_[0];
    printf "X-FTN-Tearline: %s\n\n", $VERSION;
  }

sub out_close
  {
    close(STDOUT) if (! $devel);
  }

sub node_cmp
  {
    my (@na, @nb);

    @na = split('[:/.]', $::a);
    @nb = split('[:/.]', $::b);

    # zone
    return -1 if ($na[0] < $nb[0]);
    return 1  if ($na[0] > $nb[0]);

    # net
    return -1 if ($na[1] < $nb[1]);
    return 1  if ($na[1] > $nb[1]);

    # node
    return -1 if ($na[2] < $nb[2]);
    return 1  if ($na[2] > $nb[2]);

    #point
    return -1 if ($na[3] < $nb[3]);
    return 1  if ($na[3] > $nb[3]);

    return 0;
  }
