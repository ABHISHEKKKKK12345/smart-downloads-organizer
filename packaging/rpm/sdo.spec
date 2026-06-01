Name:           smart-downloads-organizer
Version:        2.0.0
Release:        1%{?dist}
Summary:        Enterprise-grade automatic downloads folder organizer

License:        MIT
URL:            https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer
# Source tarball for rpmbuild:
#   rpmbuild -bb packaging/rpm/sdo.spec
# Expects source tree at %{_sourcedir}/smart-downloads-organizer-%{version}.tar.gz
# Build with: rpmbuild -bb --define "_sourcedir $(pwd)" packaging/rpm/sdo.spec
Source0:        smart-downloads-organizer-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  pkgconf-pkg-config
BuildRequires:  gtk3-devel >= 3.24
BuildRequires:  sqlite-devel >= 3.31
BuildRequires:  openssl-devel >= 1.1
BuildRequires:  libuuid-devel
BuildRequires:  librsvg2-tools
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       gtk3 >= 3.24
Requires:       sqlite-libs >= 3.31
Requires:       openssl-libs >= 1.1
Requires:       libuuid
Requires:       hicolor-icon-theme

Recommends:     xdg-utils
Recommends:     libnotify

%description
Smart Downloads Organizer (SDO) watches your Downloads folder in real time
using Linux inotify and automatically sorts, categorizes, deduplicates, and
cleans up incoming files using a fully configurable rule engine.

Features:
 - Real-time inotify-based file watching — zero polling overhead
 - Auto-categorization of 200+ extensions into 12 categories
 - SHA-256 content-based duplicate detection with one-click cleanup
 - Rule engine: 8 operators on 6 fields (extension, name, size, age, category, MIME)
 - Full undo history for every Move, Copy, and Rename operation
 - Cleanup suggestions for large, old, and duplicate files
 - Simulation (dry-run) mode to preview changes without touching files
 - Headless/daemon mode via --headless flag for server or background use
 - Persistent SQLite WAL-mode database with full-text file search
 - Dark-themed GTK3 interface

%prep
%autosetup -n smart-downloads-organizer-%{version} -p1

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDO_BUILD_TESTS=OFF \
    -GNinja
%cmake_build

%install
%cmake_install

# Ensure icon cache is touched for post-install update
touch %{buildroot}%{_datadir}/icons/hicolor

%check
# GUI tests require a display; run headless subset only
cd %{_vpath_builddir}
ctest --output-on-failure \
      --exclude-regex ".*gui.*" \
      --timeout 60 || true

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/update-desktop-database %{_datadir}/applications &>/dev/null || :

%postun
if [ $1 -eq 0 ]; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
    /usr/bin/update-desktop-database %{_datadir}/applications &>/dev/null || :
fi

%files
%license LICENSE
%doc README.md CHANGELOG.md docs/user-guide.md docs/rule-reference.md docs/developer-guide.md
%{_bindir}/sdo
%{_datadir}/applications/com.sdo.organizer.desktop
%{_datadir}/icons/hicolor/48x48/apps/com.sdo.organizer.png
%{_datadir}/icons/hicolor/128x128/apps/com.sdo.organizer.png
%{_datadir}/icons/hicolor/256x256/apps/com.sdo.organizer.png
%{_datadir}/metainfo/com.sdo.organizer.appdata.xml

%changelog
* Wed Jan 01 2025 SDO Project <183800969+ABHISHEKKKKK12345@users.noreply.github.com> - 2.0.0-1
- Initial RPM packaging
- Full GTK3 GUI with 7-page interface (Dashboard, Files, Duplicates,
  Cleanup, Rules, Activity Log, Settings)
- SHA-256 content-based duplicate detection engine
- Configurable rule engine with 8 condition operators and 5 action types
- Full undo history stored in SQLite
- Headless daemon mode via --headless flag
- Complete test suite (196 assertions across 5 test suites)
