Name: cronutils
Version: 1.1
Release: 1%{?dist}
Summary: Utilities to assist running batch processing jobs.

Group: System Environment/Base
License: ASL 2.0
URL: http://code.google.com/p/cronutils/
Source0: http://cronutils.googlecode.com/files/%{name}-%{version}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
A set of utilities to complement batch processing jobs, such as those run from
cron, by limiting concurrent execution of jobs, setting hard limits on the
runtime of a job, and recording execution statistics of a completed job.

%prep
%setup -q


%build
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT prefix=usr


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_mandir}/man1/*
