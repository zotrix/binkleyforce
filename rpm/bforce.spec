Summary: Bforce, Fidonet mailer
Name: bforce
Version: 0.22.8
Release: ugenk4
Copyright: GPL
Group: Fidonet/mailer
Source0: bforce-%{version}.%{release}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root

%description
BFORCE is a FTN mailer. Supports PSTN and binkp sessions.

%prep
#%setup -q -n %{name}-%{version}.%{release}
cd source
./configure --prefix=/usr --disable-log-passwd --sysconfdir=/etc/bforce --bindir=/usr/bin --with-owner=uucp --with-group=news
	    	    	    
%build
cd source
make

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/etc/bforce
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/sbin
mkdir -p $RPM_BUILD_ROOT/var/log/bforce
mkdir -p $RPM_BUILD_ROOT/var/spool/fido/bt/pin
mkdir -p $RPM_BUILD_ROOT/var/spool/fido/bt/in
mkdir -p $RPM_BUILD_ROOT/var/spool/fido/ndl
mkdir -p $RPM_BUILD_ROOT/var/spool/fido/bforce


install -o uucp -g news source/bin/bforce	$RPM_BUILD_ROOT/usr/bin/bforce
install -o uucp -g news source/bin/bfindex	$RPM_BUILD_ROOT/usr/bin/bfindex
install -o uucp -g news source/bin/bfstat	$RPM_BUILD_ROOT/usr/bin/bfstat
install -o uucp -g news source/bin/nlookup	$RPM_BUILD_ROOT/usr/bin/nlookup
install -o uucp -g news examples/bforce.conf	$RPM_BUILD_ROOT/etc/bforce/bforce.conf.sample
install -o uucp -g news examples/bforce.passwd	$RPM_BUILD_ROOT/etc/bforce/bforce.passwd.sample
install -o uucp -g news examples/bforce.subst	$RPM_BUILD_ROOT/etc/bforce/bforce.subst.sample
install -o uucp -g news examples/freq.aliases	$RPM_BUILD_ROOT/etc/bforce/freq.aliases.sample
install -o uucp -g news examples/freq.dirs	$RPM_BUILD_ROOT/etc/bforce/freq.dirs.sample
install -m755 -o uucp -g news contrib/outman	$RPM_BUILD_ROOT/usr/bin/outman


%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc README README.kst CHANGES CHANGES.kst CHANGES.ugenk COPYING INSTALL.ru README.ugenk SYSLOG TODO
%defattr(-, root, root)

%attr(550,uucp,news) /usr/bin/bforce
%attr(550,uucp,news) /usr/bin/bfindex
%attr(550,uucp,news) /usr/bin/bfstat
%attr(550,uucp,news) /usr/bin/nlookup
%attr(550,uucp,news) /usr/bin/outman
%attr(644,root,root) /usr/share/doc/bforce-0.22.8/*
%dir %attr(770,uucp,news)  /var/log/bforce
%dir %attr(770,uucp,news)  /var/spool/fido/ndl
%attr(775,uucp,news) /var/spool/fido/bt
%config %attr(600,uucp,news) /etc/bforce/bforce.conf.sample
%config %attr(600,uucp,news) /etc/bforce/bforce.subst.sample
%config %attr(600,uucp,news) /etc/bforce/bforce.passwd.sample
%config %attr(600,uucp,news) /etc/bforce/freq.aliases.sample
%config %attr(600,uucp,news) /etc/bforce/freq.dirs.sample

