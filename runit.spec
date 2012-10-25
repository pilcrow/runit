%define svscandir /service

Summary: runit is a cross-platform Unix init scheme with service supervision
Name: runit
Version: 2.1.1.2012102301
Release: 1
License: BSD
Group: System Environment/Daemons
URL: http://smarden.org/runit/index.html
Source0: http://smarden.org/runit/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
runit is a cross-platform Unix init scheme with service supervision, a
replacement for sysvinit, and other init schemes. It runs on
GNU/Linux, *BSD, MacOSX, Solaris, and can easily be adapted to other
Unix operating systems.

%package daemontools
Conflicts: daemontools
Group: System Environment/Daemons
Summary: runit compatibility with DJB's daemontools utilities

%description daemontools
runit-daemontools is a compatibility layer for daemontools-style process
supervision.

%prep
%setup -n admin/%{name}-%{version} -q -a 0

%build
echo %{__cc} %{optflags} -I. >src/conf-cc
echo %{__cc} %{optflags} -I. -s >src/conf-ld
mkdir compile
echo %{buildroot}/usr >compile/home
./package/compile

%install
rm -rf $RPM_BUILD_ROOT
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && %{__rm} -rf $RPM_BUILD_ROOT
%{__install} -m 0755 -d ${RPM_BUILD_ROOT}/sbin
%{__install} -m 0755 -d ${RPM_BUILD_ROOT}%{_sysconfdir}/%{name}
#%{__install} -m 0755 -d ${RPM_BUILD_ROOT}%{_datadir}/doc
%{__install} -m 0755 -d ${RPM_BUILD_ROOT}%{svscandir}
%{__install} -m 0755 -d ${RPM_BUILD_ROOT}%{_localstatedir}/service

for i in `cat package/commands`; do
  %{__install} -m 0755 command/$i ${RPM_BUILD_ROOT}/sbin
done

for i in 8; do 
  %{__install} -m 0755 -d ${RPM_BUILD_ROOT}%{_mandir}/man$i
  for j in man/*.$i; do
#   gzip -c $j > ${j}.gz
#   %{__install} -m 0644 ${j}.gz ${RPM_BUILD_ROOT}%{_mandir}/man$i
    %{__install} -m 0644 ${j} ${RPM_BUILD_ROOT}%{_mandir}/man$i
  done
done

for DT_COMPAT in setuidgid envuidgid envdir pgrphack softlimit setlock; do
  %{__ln_s} /sbin/chpst ${RPM_BUILD_ROOT}/sbin/${DT_COMPAT}
done

%{__install} -m 0755 ./etc/debian/2 ${RPM_BUILD_ROOT}%{_sysconfdir}/%{name}/2

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}
%doc doc/*.html
/sbin/runit
/sbin/chpst
/sbin/runit-init
/sbin/runsv
/sbin/runsvchdir
/sbin/runsvdir
/sbin/sv
/sbin/svlogd
/sbin/utmpset
%{_mandir}/man8/*

%files daemontools
/sbin/setuidgid
/sbin/envuidgid
/sbin/envdir
/sbin/pgrphack
/sbin/softlimit
/sbin/setlock

%changelog
* Thu Oct 25 2012 Mike Pomraning <devnull@pilcrow.madison.wi.us> - 2.1.1.2012102301-1
- RPMify new modifications
* Sun May 23 2010 Mike Pomraning <pkg@pilcrow.madison.wi.us> - 2.1.1-1
- Initial packaging, without init replacement
