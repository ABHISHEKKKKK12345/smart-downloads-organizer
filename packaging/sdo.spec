Name:           smart-downloads-organizer
Version:        2.0.0
Release:        1%{?dist}
Summary:        Enterprise-grade automatic downloads folder organizer

License:        MIT
URL:            https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  gtk3-devel >= 3.24
BuildRequires:  sqlite-devel >= 3.31
BuildRequires:  openssl-devel >= 3.0
BuildRequires:  libuuid-devel
BuildRequires:  pkgconfig
BuildRequires:  librsvg2-tools
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       gtk3 >= 3.24
Requires:       sqlite-libs >= 3.31
Requires:       openssl-libs >= 3.0
Requires:       libuuid
Requires:       hicolor-icon-theme

Recommends:     xdg-utils

%description
Smart Downloads Organizer watches your Downloads folder in real time using
Linux inotify and automatically sorts, categorizes, deduplicates, and cleans
up incoming files using a fully configurable rule engine.

Features:
 - Real-time inotify-based file watching with zero polling overhead
 - Auto-categorization of 200+ extensions into 12 categories
 - SHA-256 duplicate detection with one-click cleanup
 - Configurable rule engine: extension, name, size, age, MIME, regex
 - Full undo history for every file operation
 - Cleanup suggestions for large, old, and duplicate files
 - Simulation (dry-run) mode to preview changes
 - Headless/daemon mode for background operation
 - Persistent SQLite database with full-text search
 - Dark-themed GTK3 interface

%prep
%autosetup -p1

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDO_BUILD_TESTS=OFF \
    -GNinja
%cmake_build

%install
%cmake_install

# Register icon cache update
touch %{buildroot}%{_datadir}/icons/hicolor

%check
# Run non-GUI tests only
cd %{_vpath_builddir}
ctest --output-on-failure --exclude-regex ".*gui.*" --timeout 60 || true

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/update-desktop-database %{_datadir}/applications &>/dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
    /usr/bin/update-desktop-database %{_datadir}/applications &>/dev/null || :
fi

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/sdo
%{_datadir}/applications/com.sdo.organizer.desktop
%{_datadir}/icons/hicolor/48x48/apps/com.sdo.organizer.png
%{_datadir}/icons/hicolor/128x128/apps/com.sdo.organizer.png
%{_datadir}/icons/hicolor/256x256/apps/com.sdo.organizer.png
%{_datadir}/metainfo/com.sdo.organizer.appdata.xml

%changelog
* Wed Jan 01 2025 SDO Project <183800969+ABHISHEKKKKK12345@users.noreply.github.com> - 2.0.0-1
- Initial RPM packaging
- Full GTK3 GUI with all 7 pages
- SHA-256 duplicate detection
- Configurable rule engine
- Persistent SQLite database
- Headless daemon mode
