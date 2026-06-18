# Feodra spec for HGS stable releases

%global debug_package %{nil}
%global version VERSION_PLACEHOLDER
%global pkg_summary HyprGlassShell - Material 3 inspired shell for Wayland compositors

Name:           hgs
Version:        %{version}
Release:        RELEASE_PLACEHOLDER%{?dist}
Summary:        %{pkg_summary}

License:        MIT
URL:            https://github.com/CoastLineSec/HyprGlassShell

Source0:        hgs-qml.tar.gz

BuildRequires:  gzip
BuildRequires:  wget
BuildRequires:  systemd-rpm-macros

Requires:       (quickshell or quickshell-git)
Requires:       accountsservice
Requires:       hgs-cli = %{version}-%{release}
Requires:       dgop

Recommends:     cava
Recommends:     hgssearch
Recommends:     matugen
Recommends:     NetworkManager
Recommends:     qt6-qtmultimedia
Suggests:       cups-pk-helper
Suggests:       qt6ct

%description
HyprGlassShell (HGS) is a modern Wayland desktop shell built with Quickshell
and optimized for the Hyprland compositor. Features notifications,
app launcher, wallpaper customization, and fully customizable with plugins.

Includes auto-theming for GTK/Qt apps with matugen, 20+ customizable widgets,
process monitoring, notification center, clipboard history, dock, control center,
lock screen, and comprehensive plugin system.

%package -n hgs-cli
Summary:        HyprGlassShell CLI tool
License:        MIT
URL:            https://github.com/CoastLineSec/HyprGlassShell

%description -n hgs-cli
Command-line interface for HyprGlassShell configuration and management.
Provides native DBus bindings, NetworkManager integration, and system utilities.

%prep
%setup -q -c -n hgs-qml

case "%{_arch}" in
  x86_64)
    ARCH_SUFFIX="amd64"
    ;;
  aarch64)
    ARCH_SUFFIX="arm64"
    ;;
  *)
    echo "Unsupported architecture: %{_arch}"
    exit 1
    ;;
esac

# Download hgs-cli for target architecture
wget -O %{_builddir}/hgs-cli.gz "https://github.com/CoastLineSec/HyprGlassShell/releases/latest/download/hgs-distropkg-${ARCH_SUFFIX}.gz" || {
  echo "Failed to download hgs-cli for architecture %{_arch}"
  exit 1
}
gunzip -c %{_builddir}/hgs-cli.gz > %{_builddir}/hgs-cli
chmod +x %{_builddir}/hgs-cli

%build

%install
install -Dm755 %{_builddir}/hgs-cli %{buildroot}%{_bindir}/hgs

# Shell completions
install -d %{buildroot}%{_datadir}/bash-completion/completions
install -d %{buildroot}%{_datadir}/zsh/site-functions
install -d %{buildroot}%{_datadir}/fish/vendor_completions.d
%{_builddir}/hgs-cli completion bash > %{buildroot}%{_datadir}/bash-completion/completions/hgs || :
%{_builddir}/hgs-cli completion zsh > %{buildroot}%{_datadir}/zsh/site-functions/_hgs || :
%{_builddir}/hgs-cli completion fish > %{buildroot}%{_datadir}/fish/vendor_completions.d/hgs.fish || :

install -Dm644 %{_builddir}/hgs-qml/assets/systemd/hgs.service %{buildroot}%{_userunitdir}/hgs.service

install -Dm644 %{_builddir}/hgs-qml/assets/hgs-open.desktop %{buildroot}%{_datadir}/applications/hgs-open.desktop
install -Dm644 %{_builddir}/hgs-qml/assets/hgslogo.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/hgslogo.svg

install -dm755 %{buildroot}%{_datadir}/quickshell/hgs
cp -r %{_builddir}/hgs-qml/* %{buildroot}%{_datadir}/quickshell/hgs/

rm -rf %{buildroot}%{_datadir}/quickshell/hgs/.git*
rm -f %{buildroot}%{_datadir}/quickshell/hgs/.gitignore
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/.github
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/distro

echo "%{version}" > %{buildroot}%{_datadir}/quickshell/hgs/VERSION

%posttrans
# Signal running HGS instances to reload
pkill -USR1 -x hgs >/dev/null 2>&1 || :

%files
%license LICENSE
%doc README.md CONTRIBUTING.md
%{_datadir}/quickshell/hgs/
%{_userunitdir}/hgs.service
%{_datadir}/applications/hgs-open.desktop
%{_datadir}/icons/hicolor/scalable/apps/hgslogo.svg

%files -n hgs-cli
%{_bindir}/hgs
%{_datadir}/bash-completion/completions/hgs
%{_datadir}/zsh/site-functions/_hgs
%{_datadir}/fish/vendor_completions.d/hgs.fish

%changelog
* CHANGELOG_DATE_PLACEHOLDER AvengeMedia <contact@avengemedia.com> - VERSION_PLACEHOLDER-RELEASE_PLACEHOLDER
- Stable release VERSION_PLACEHOLDER
- Built from GitHub release
