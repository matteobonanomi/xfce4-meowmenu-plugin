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

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  gcc-c++
BuildRequires:  gtk3-devel
BuildRequires:  glib2-devel
BuildRequires:  garcon-devel
BuildRequires:  xfce4-panel-devel
BuildRequires:  libxfce4ui-devel
BuildRequires:  libxfce4util-devel
BuildRequires:  xfconf-devel
BuildRequires:  accountsservice-devel
BuildRequires:  gtk-layer-shell-devel
BuildRequires:  gettext

Requires:       xfce4-panel
Recommends:     accountsservice
Recommends:     gtk-layer-shell

%description
MeowMenu is a fork of the Xfce Whisker Menu that keeps the familiar panel
launcher feel while bringing a cleaner modern look, a more capable search
bar, and extra customization options. Built for Xubuntu 26.04 with
Xfce 4.20.x.

%prep
%autosetup -n %{name}-%{version}

%build
%meson
%meson_build

%install
%meson_install
%find_lang %{plugin_name}

%files -f %{plugin_name}.lang
%license COPYING
%doc README.md NEWS
%{_libdir}/xfce4/panel/plugins/lib*.so
%{_datadir}/xfce4/panel/plugins/*.desktop
%{_datadir}/icons/hicolor/*/apps/*meowmenu*
%{_datadir}/applications/*meowmenu*.desktop

%changelog
* Tue May 19 2026 Matteo Bonanomi <mbonanomi.dev@proton.me> - 0.3.3-1
- Seed entry. Regenerated from NEWS at build time by packaging.yml.
