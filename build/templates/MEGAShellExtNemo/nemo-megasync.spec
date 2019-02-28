Name:       nemo-megasync
Version:    EXT_VERSION
Release:	1%{?dist}
Summary:	Easy automated syncing between your computers and your MEGA cloud drive
License:	Freeware
Group:		Applications/Others
Url:		https://mega.nz
Source0:	nemo-megasync_%{version}.tar.gz
Vendor:		MEGA Limited
Packager:	MEGA Linux Team <linux@mega.co.nz>

BuildRequires:  qt-devel, glib2-devel, nemo-devel, gnome-common
BuildRequires:  pkgconfig(libnemo-extension)
BuildRequires:	hicolor-icon-theme, gnome-shell
%if 0%{?rhel_version} 
BuildRequires: redhat-logos
%endif
%if 0%{?fedora_version} 
BuildRequires: fedora-logos
%endif
%if 0%{?scientificlinux_version} 
BuildRequires: sl-logos, gcc-c++
%endif

Requires:       nemo, megasync >= 3.5

%description
Secure:
Your data is encrypted end to end. Nobody can intercept it while in storage or in transit.

Flexible:
Sync any folder from your PC to any folder in the cloud. Sync any number of folders in parallel.

Fast:
Take advantage of MEGA's high-powered infrastructure and multi-connection transfers.

Generous:
Store up to 50 GB for free!

%prep
%setup -q

%build
export DESKTOP_DESTDIR=$RPM_BUILD_ROOT/usr

%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version} || 0%{?scientificlinux_version}
qmake-qt4
%else
qmake
%endif

if [ 0$(head /usr/share/doc/nemo/NEWS -n 1 | awk '{print $NF}' | awk -F':' '{print $1}' | awk -F "." '{FS=".";print $1*10000+$2*100+$3}') -gt 31503 ]; then 
    for i in data/emblems/64x64/*smaller.png; do mv $i ${i/-smaller/}; done
    echo "NEWER NEMO REQUIRES SMALLER OVERLAY ICONS"    
else
    rm data/emblems/64x64/*smaller.png
    echo "OLDER NEMO DOES NOT REQUIRE SMALLER OVERLAY ICONS"
fi
%if 0%{?fedora_version} >= 27
#tweak to have debug symbols to stripe: for some reason they seem gone by default in Fedora 27,
#   causing "gdb-add-index: No index was created for ..." which lead to error "Empty %files file ....debugsourcefiles.list"
sed "s# gcc# gcc -g#g" -i Makefile
%endif
make

%install
make install

export EXTENSIONSDIR=$(pkg-config --variable=extensiondir libnemo-extension)
mkdir -p %{buildroot}$EXTENSIONSDIR
%{__install} libMEGAShellExtNemo.so -D %{buildroot}$EXTENSIONSDIR

# clean up
rm -fr $RPM_BUILD_ROOT/usr/share/icons/hicolor/icon-theme.cache || true

%post
%if 0%{?suse_version} >= 1140
%icon_theme_cache_post
%else
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
%endif
# restart Nemo
UPDATENOTIFIERDIR=/var/lib/update-notifier/user.d
echo "Please restart all running instances of Nemo."

if [ -d $UPDATENOTIFIERDIR ] ; then
        cat > $UPDATENOTIFIERDIR/megasync-install-notify <<DATA
Name: Nemo Restart Required
Priority: High
Terminal: False
Command: nemo -q
DontShowAfterReboot: True
ButtonText: _Restart Nemo
DisplayIf: pgrep -x nemo -U \$(id -u) > /dev/null
OnlyAdminUsers: False
Description:
 MEGAsync requires Nemo to be restarted to function properly.
DATA
fi

%postun
%if 0%{?suse_version} >= 1140
%icon_theme_cache_postun
%else
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi
%endif
# restart Nemo
UPDATENOTIFIERDIR=/var/lib/update-notifier/user.d
echo "Please restart all running instances of Nemo."

if [ -d $UPDATENOTIFIERDIR ] ; then
        cat > $UPDATENOTIFIERDIR/megasync-install-notify <<DATA
Name: Nemo Restart Required
Priority: High
Terminal: False
Command: nemo -q
DontShowAfterReboot: True
ButtonText: _Restart Nemo
DisplayIf: pgrep -x nemo -U \$(id -u) > /dev/null
OnlyAdminUsers: False
Description:
 MEGAsync requires Nemo to be restarted to function properly.
DATA
fi


%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version} || 0%{?scientificlinux_version}
%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
%endif

%clean
%{?buildroot:%__rm -rf "%{buildroot}"}

%files
%defattr(-,root,root)
%{_libdir}/nemo/extensions-3.0/libMEGAShellExtNemo.so
%{_datadir}/icons/hicolor/*/*/mega-*.icon
%{_datadir}/icons/hicolor/*/*/mega-*.png

%changelog

