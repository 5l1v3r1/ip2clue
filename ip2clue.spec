Summary:	IP to country (and other information) tools
Name:		ip2clue
Version:	0.0.92
Release:	1
License:	GPLv3+
Group:		Applications/Internet
Source:		http://kernel.embedromix.ro/us/ip2clue/%{name}-%{version}.tar.gz
URL:		http://kernel.embedromix.ro/us/
BuildRoot:	%{_tmppath}/%{name}-%{version}-buildroot
BuildRequires:	Conn >= 1.0.31
Requires:	Conn >= 1.0.31, gzip, wget, unzip, crontabs

%description
A set of tools for IP 2 country look up operations. It has a fast daemon that
caches and searches for information and tools on the client side.

%prep
%setup -q

%build
%configure
make

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}
make install DESTDIR=${RPM_BUILD_ROOT}

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr (-,root,root)
%{_sbindir}/ip2clued
%{_bindir}/ip2clue
%{_sysconfdir}/ip2clue/*
%{_sysconfdir}/rc.d/init.d/ip2clued
%{_sysconfdir}/cron.daily/*
%{_datadir}/ip2clue/*
%dir %{_var}/cache/ip2clue
%doc README Changelog TODO LICENSE clients

%post
/sbin/chkconfig --add ip2clued

%preun
if [ "$1" = "0" ]; then
	/sbin/service ip2clued stop > /dev/null 2>&1
	/sbin/chkconfig --del ip2clued
fi

%changelog
* Fri May 28 2010 <Catalin(ux) M. BOIE> - 0.0.90
- First version.
