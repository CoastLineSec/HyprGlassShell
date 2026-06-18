# Spec for HGS - uses rpkg macros for git builds

%global debug_package %{nil}
%global version {{{ git_repo_version }}}
%global pkg_summary HyprGlassShell - Material 3 inspired shell for Wayland compositors
%global go_toolchain_version 1.26.1

Name:           hgs
Epoch:          2
Version:        %{version}
Release:        1%{?dist}
Summary:        %{pkg_summary}

License:        MIT
URL:            https://github.com/CoastLineSec/HyprGlassShell
VCS:            {{{ git_repo_vcs }}}
Source0:        {{{ git_repo_pack }}}
Source1:        https://go.dev/dl/go%{go_toolchain_version}.linux-amd64.tar.gz
Source2:        https://go.dev/dl/go%{go_toolchain_version}.linux-arm64.tar.gz

BuildRequires:  git-core
BuildRequires:  gzip
BuildRequires:  make
BuildRequires:  systemd-rpm-macros

# Core requirements
Requires:       (quickshell-git or quickshell)
Requires:       accountsservice
Requires:       hgs-cli = %{epoch}:%{version}-%{release}
Requires:       dgop

# Core utilities (Recommended for HGS functionality)
Recommends:     cava
Recommends:     hgssearch
Recommends:     matugen
Recommends:     quickshell-git

# Recommended system packages
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
{{{ git_repo_setup_macro }}}

%build
# Build HGS CLI from source (core/subdirectory)
VERSION="%{version}"
COMMIT=$(echo "%{version}" | grep -oP '[a-f0-9]{7,}' | head -n1 || echo "unknown")

# Use pinned bundled Go toolchain (deterministic across chroots)
case "%{_arch}" in
  x86_64)
    GO_TARBALL="%{_sourcedir}/go%{go_toolchain_version}.linux-amd64.tar.gz"
    ;;
  aarch64)
    GO_TARBALL="%{_sourcedir}/go%{go_toolchain_version}.linux-arm64.tar.gz"
    ;;
  *)
    echo "Unsupported architecture for bundled Go: %{_arch}"
    exit 1
    ;;
esac

rm -rf .go
tar -xzf "$GO_TARBALL"
mv go .go
export GOROOT="$PWD/.go"
export PATH="$GOROOT/bin:$PATH"
export GOTOOLCHAIN=local
go version

cd core
make dist VERSION="$VERSION" COMMIT="$COMMIT"

%install
# Install hgs-cli binary (built from source)
case "%{_arch}" in
  x86_64)
    HGS_BINARY="hgs-linux-amd64"
    ;;
  aarch64)
    HGS_BINARY="hgs-linux-arm64"
    ;;
  *)
    echo "Unsupported architecture: %{_arch}"
    exit 1
    ;;
esac

install -Dm755 core/bin/${HGS_BINARY} %{buildroot}%{_bindir}/hgs

# Shell completions
install -d %{buildroot}%{_datadir}/bash-completion/completions
install -d %{buildroot}%{_datadir}/zsh/site-functions
install -d %{buildroot}%{_datadir}/fish/vendor_completions.d
core/bin/${HGS_BINARY} completion bash > %{buildroot}%{_datadir}/bash-completion/completions/hgs || :
core/bin/${HGS_BINARY} completion zsh > %{buildroot}%{_datadir}/zsh/site-functions/_hgs || :
core/bin/${HGS_BINARY} completion fish > %{buildroot}%{_datadir}/fish/vendor_completions.d/hgs.fish || :

# Install systemd user service
install -Dm644 assets/systemd/hgs.service %{buildroot}%{_userunitdir}/hgs.service

install -Dm644 assets/hgs-open.desktop %{buildroot}%{_datadir}/applications/hgs-open.desktop
install -Dm644 assets/hgslogo.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/hgslogo.svg

# Install shell files to shared data location
install -dm755 %{buildroot}%{_datadir}/quickshell/hgs
cp -r quickshell/* %{buildroot}%{_datadir}/quickshell/hgs/

# Remove build files
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/.git*
rm -f %{buildroot}%{_datadir}/quickshell/hgs/.gitignore
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/.github
rm -rf %{buildroot}%{_datadir}/quickshell/hgs/distro

%posttrans
# Signal running HGS instances to reload
pkill -USR1 -x hgs >/dev/null 2>&1 || :

%files
%license LICENSE
%doc CONTRIBUTING.md
%doc quickshell/README.md
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
{{{ git_repo_changelog }}}
