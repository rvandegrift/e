%define __os_install_post /usr/lib/rpm/brp-compress
%define debug_package %{nil}
%{!?_rel:%{expand:%%global _rel 0.enl%{?dist}}}
%define _missing_doc_files_terminate_build 0

%if %(systemctl --version | head -1 | cut -d' ' -f2) >= 209
%{expand:%%global have_systemd 1}
%endif

%{expand:%%global ac_enable_systemd --%{?have_systemd:en}%{!?have_systemd:dis}able-systemd}

Summary: The Enlightenment window manager
Name: enlightenment
Version: 0.21.3
Release: %{_rel}
License: BSD
Group: User Interface/Desktops
URL: http://www.enlightenment.org/
Source: ftp://ftp.enlightenment.org/pub/enlightenment/%{name}-%{version}.tar.gz
Packager: %{?_packager:%{_packager}}%{!?_packager:Michael Jennings <mej@eterm.org>}
Vendor: %{?_vendorinfo:%{_vendorinfo}}%{!?_vendorinfo:The Enlightenment Project (http://www.enlightenment.org/)}
Distribution: %{?_distribution:%{_distribution}}%{!?_distribution:%{_vendor}}
#BuildSuggests: xorg-x11-devel, XFree86-devel, libX11-devel
BuildRequires: efl-devel >= 1.17.0
BuildRequires: libxcb-devel, xcb-util-devel
Prefix: %{_prefix}
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Enlightenment is a window manager.


%package devel
Summary: Development headers for Enlightenment. 
Group: User Interface/Desktops
Requires: %{name} = %{version}
Requires: efl-devel >= 1.17.0

%description devel
Development headers for Enlightenment.


%prep
%setup -q


%build
%{configure} --prefix=%{_prefix} %{ac_enable_systemd} --with-profile=FAST_PC CFLAGS="-O0 -ggdb3"
%{__make} %{?_smp_mflags} %{?mflags}


%install
%{__make} %{?mflags_install} DESTDIR=$RPM_BUILD_ROOT install
test -x `which doxygen` && sh gendoc || :
find $RPM_BUILD_ROOT%{_prefix} -name '*.la' -print0 | xargs -0 rm -f

%{find_lang} %{name}


%clean
test "x$RPM_BUILD_ROOT" != "x/" && rm -rf $RPM_BUILD_ROOT


%post
/sbin/ldconfig


%postun
/sbin/ldconfig


%files -f %{name}.lang
%defattr(-, root, root)
%doc AUTHORS COPYING README
%dir %{_sysconfdir}/enlightenment
%config(noreplace) %{_sysconfdir}/enlightenment/*
%config(noreplace) %{_sysconfdir}/xdg/menus/e-applications.menu
%{_bindir}/emixer
%{_bindir}/enlightenment
%{_bindir}/enlightenment_*
%{_libdir}/%{name}/
%{_datadir}/%{name}/
%{_datadir}/applications/*.desktop
%{_datadir}/pixmaps/e*
%{_datadir}/xsessions/%{name}.desktop
%if %{?have_systemd:1}0
%{_prefix}/lib/systemd/*/*.service
%endif

%files devel
%defattr(-, root, root)
%{_includedir}/enlightenment/
%{_libdir}/pkgconfig/*.pc


%changelog
