# NOTE: `Version:` and the `%changelog` top entry are rewritten by the
# packaging workflow (.github/workflows/packaging.yml) from the top entry of
# NEWS at build time via `tools/news-version.py`. Do NOT hand-edit those
# fields; the placeholder values below exist only so `rpmbuild -bs` accepts
# the spec when run outside CI.

%global plugin_name xfce4-meowmenu-plugin

Name:           %{plugin_name}
Version:        0.3.3
Release:        1%{?dist}
Summary:        Modern menu launcher plugin for the Xfce panel

License:        GPLv2+
URL:            https://github.com/matteobonanomi/xfce4-meowmenu-plugin
Source0:        %{name}-%{version}.tar.gz

Packager:       Matteo Bonanomi <mbonanomi.dev@proton.me>

# NOTE: BuildRequires are expressed via pkgconfig(...) / path capabilities
# so the same spec works on Fedora and openSUSE Leap. Binary package names
# for Xfce devel libraries diverge between distros (e.g. garcon-devel vs
# libgarcon-1-devel, xfconf-devel vs libxfconf-devel, ninja-build vs ninja,
# pkgconf-pkg-config vs pkgconfig); .pc filenames and /usr/bin paths are
# stable and match meson.build.
BuildRequires:  meson
BuildRequires:  gcc-c++
BuildRequires:  /usr/bin/ninja
BuildRequires:  /usr/bin/pkg-config
BuildRequires:  /usr/bin/msgfmt
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(garcon-1)
BuildRequires:  pkgconfig(libxfce4panel-2.0)
BuildRequires:  pkgconfig(libxfce4ui-2)
BuildRequires:  pkgconfig(libxfce4util-1.0)
BuildRequires:  pkgconfig(exo-2)
BuildRequires:  pkgconfig(libxfconf-0)
BuildRequires:  pkgconfig(accountsservice)
BuildRequires:  pkgconfig(gtk-layer-shell-0)

Requires:       xfce4-panel
Recommends:     accountsservice
Recommends:     gtk-layer-shell

%description
MeowMenu is a panel-plugin launcher for the Xfce desktop. It is a
standalone project that originated as a fork of Xfce's Whisker Menu
and keeps the familiar panel-launcher feel while bringing a cleaner
modern look and a more capable search bar. MeowMenu coexists with
Whisker Menu and does not replace it. Built for Xubuntu 26.04 with
Xfce 4.20.x.

%prep
%autosetup -n %{name}-%{version}

%build
%meson
%meson_build

%install
%meson_install
# NOTE: GETTEXT_PACKAGE matches the meson project() name
# (feature 010 — xfce4-meowmenu-plugin). Locale .mo files install under
# that name, so %find_lang must use it.
%find_lang xfce4-meowmenu-plugin

%files -f xfce4-meowmenu-plugin.lang
%license COPYING
%doc README.md NEWS
%{_bindir}/xfce4-popup-meowmenu
%{_libdir}/xfce4/panel/plugins/lib*.so
%{_datadir}/xfce4/panel/plugins/*.desktop
%{_datadir}/icons/hicolor/*/apps/*meowmenu*
%{_datadir}/meowmenu/presets/
%{_datadir}/xfce4-meowmenu-plugin/
%{_datadir}/metainfo/xfce4-meowmenu-plugin.appdata.xml
%{_mandir}/man1/xfce4-popup-meowmenu.1*

%changelog
* Tue May 19 2026 Matteo Bonanomi <mbonanomi.dev@proton.me> - 0.3.3-1
- Seed entry. Regenerated from NEWS at build time by packaging.yml.
