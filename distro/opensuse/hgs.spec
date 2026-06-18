# Spec for HGS for OpenSUSE/OBS

%global debug_package %{nil}

Name:           hgs
Version:        1.2.3
Release:        1%{?dist}
Summary:        HyprGlassShell - Material 3 inspired shell for Wayland compositors

License:        MIT
URL:            https://github.com/CoastLineSec/HyprGlassShell
Source0:        hgs-source.tar.gz
Source1:        hgs-distropkg-amd64.gz
Source2:        hgs-distropkg-arm64.gz

BuildRequires:  gzip
BuildRequires:  systemd-rpm-macros

# Core requirements
Requires:       (quickshell or quickshell-git)
Requires:       accountsservice
Requires:       dgop

# Core utilities (Highly recommended for HGS functionality)
Recommends:     cava
Recommends:     hgssearch
Recommends:     matugen
Recommends:     NetworkManager
Recommends:     qt6-qtmultimedia
Suggests:       cups-pk-helper
Suggests:       qt6ct

%description
HyprGlassShell (HGS) is a modern Wayland desktop shell built with Quickshell
and optimized for the Hyprland compositor. Features
notifications, app launcher, wallpaper customization, and plugin system.

Includes auto-theming for GTK/Qt apps with matugen, 20+ customizable widgets,
process monitoring, notification center, clipboard history, dock, control center,
lock screen, and comprehensive plugin system.

%prep
%setup -q -n HyprGlassShell-%{version}

%ifarch x86_64
gunzip -c %{SOURCE1} > hgs
%endif
%ifarch aarch64
gunzip -c %{SOURCE2} > hgs
%endif
chmod +x hgs

%build

%install
install -Dm755 hgs %{buildroot}%{_bindir}/hgs

install -d %{buildroot}%{_datadir}/bash-completion/completions
install -d %{buildroot}%{_datadir}/zsh/site-functions
install -d %{buildroot}%{_datadir}/fish/vendor_completions.d
./hgs completion bash > %{buildroot}%{_datadir}/bash-completion/completions/hgs || :
./hgs completion zsh > %{buildroot}%{_datadir}/zsh/site-functions/_hgs || :
./hgs completion fish > %{buildroot}%{_datadir}/fish/vendor_completions.d/hgs.fish || :

install -Dm644 assets/systemd/hgs.service %{buildroot}%{_userunitdir}/hgs.service

install -Dm644 assets/hgs-open.desktop %{buildroot}%{_datadir}/applications/hgs-open.desktop
install -Dm644 assets/hgslogo.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/hgslogo.svg

install -dm755 %{buildroot}%{_datadir}/quickshell/hgs
cp -r quickshell/* %{buildroot}%{_datadir}/quickshell/hgs/

rm -rf %{buildroot}%{_datadir}/quickshell/hgs/.git*
rm -f %{buildroot}%{_datadir}/quickshell/hgs/.gitignore
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/.github
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/distro
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/core

echo "%{version}" > %{buildroot}%{_datadir}/quickshell/hgs/VERSION

%posttrans
# Signal running HGS instances to reload
pkill -USR1 -x hgs >/dev/null 2>&1 || :

%files
%license LICENSE
%doc CONTRIBUTING.md
%doc quickshell/README.md
%{_bindir}/hgs
%dir %{_datadir}/fish
%dir %{_datadir}/fish/vendor_completions.d
%{_datadir}/fish/vendor_completions.d/hgs.fish
%dir %{_datadir}/zsh
%dir %{_datadir}/zsh/site-functions
%{_datadir}/zsh/site-functions/_hgs
%{_datadir}/bash-completion/completions/hgs
%dir %{_datadir}/quickshell
%{_datadir}/quickshell/hgs/
%{_userunitdir}/hgs.service
%{_datadir}/applications/hgs-open.desktop
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/scalable
%dir %{_datadir}/icons/hicolor/scalable/apps
%{_datadir}/icons/hicolor/scalable/apps/hgslogo.svg

%changelog
* Mon Dec 16 2025 AvengeMedia <maintainer@avengemedia.com> - 1.0.3-1
- Update to stable v1.0.3 release

* Fri Dec 12 2025 AvengeMedia <maintainer@avengemedia.com> - 1.0.2-1
- Update to stable v1.0.2 release
- Bug fixes and improvements

* Fri Nov 22 2025 AvengeMedia <maintainer@avengemedia.com> - 0.6.2-1
- Stable release build with pre-built binaries
- Multi-arch support (x86_64, aarch64)
