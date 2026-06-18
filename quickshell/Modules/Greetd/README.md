# HGS (hgs) Greeter

A greeter for [greetd](https://github.com/kennylevinsen/greetd) that follows the aesthetics of the hgs lock screen.

## Features

- **Multi user**: Login with any system user
- **hgs sync**: Sync settings with hgs for consistent styling between shell and greeter
- **Hyprland session**: The `hgs-greeter` wrapper launches a Hyprland-based greeter session.
- **Custom PAM**: Supports custom PAM configuration in `/etc/pam.d/greetd`
- **Session Memory**: Remembers last selected session and user
  - Can be disabled via `settings.json` keys: `greeterRememberLastSession` and `greeterRememberLastUser`

## Installation

### Arch Linux

Arch linux users can install [greetd-hgs-greeter-git](https://aur.archlinux.org/packages/greetd-hgs-greeter-git) from the AUR.

```bash
paru -S greetd-hgs-greeter-git
# Or with yay
yay -S greetd-hgs-greeter-git
```

### Debian / openSUSE

Official packages are available from the [CoastLineSec OBS repository](https://software.opensuse.org/download/package?package=hgs-greeter&project=home%3AAvengeMedia%3Acoastlinesec). Add the repo for your distribution and install:

```bash
# Debian 13
sudo apt install hgs-greeter   # after adding the repo

# openSUSE Tumbleweed
zypper install hgs-greeter     # after adding the repo
```

See the [Installation guide](https://coastlinesec.com/docs/hgsgreeter/installation) for full repository setup.

If you previously installed manually, remove legacy files first:

```bash
sudo rm -f /usr/local/bin/hgs-greeter
sudo rm -rf /etc/xdg/quickshell/hgs-greeter
```

Then complete setup:

```bash
hgs greeter enable
hgs greeter sync
```

#### Syncing themes (Optional)

To sync your wallpaper and theme with the greeter login screen, follow the manual setup below:

<details>
<summary>Manual theme syncing</summary>

```bash
# Add yourself to greeter group
sudo usermod -aG greeter <username>

# Set ACLs to allow greeter to traverse your directories
setfacl -m u:greeter:x ~ ~/.config ~/.local ~/.cache ~/.local/state

# Set group ownership on config directories
sudo chgrp -R greeter ~/.config/HyprGlassShell
sudo chgrp -R greeter ~/.local/state/HyprGlassShell
sudo chgrp -R greeter ~/.cache/HyprGlassShell
sudo chmod -R g+rX ~/.config/HyprGlassShell ~/.cache/HyprGlassShell ~/.cache/quickshell

# Create symlinks
sudo ln -sf ~/.config/HyprGlassShell/settings.json /var/cache/hgs-greeter/settings.json
sudo ln -sf ~/.local/state/HyprGlassShell/session.json /var/cache/hgs-greeter/session.json
sudo ln -sf ~/.cache/HyprGlassShell/hgs-colors.json /var/cache/hgs-greeter/colors.json

# Logout and login for group membership to take effect
```

</details>

### Fedora / RHEL / Rocky / Alma

Install from COPR or build the RPM:

```bash
# From COPR (when available)
sudo dnf copr enable avenge/hgs
sudo dnf install hgs-greeter

# Or build locally
cd /path/to/HyprGlassShell
rpkg local
sudo rpm -ivh x86_64/hgs-greeter-*.rpm
```

The package automatically:

- Creates the greeter user (via `systemd-sysusers` from `/usr/lib/sysusers.d/hgs-greeter.conf` for atomic/immutable compatibility, with package script fallback)
- Sets up directories and permissions
- Configures greetd with auto-detected compositor
- Applies SELinux contexts

Then complete setup:

```bash
hgs greeter enable
hgs greeter sync
```

#### Syncing themes (Optional)

Run:

```bash
hgs greeter sync
```

Then logout/login to see your wallpaper on the greeter.

### Automatic

The easiest thing is to run `hgs greeter install` or `hgs` for interactive installation.
On Debian/openSUSE, this now prefers the `hgs-greeter` package when the OBS repo is configured.

### Manual (fallback only)

Use this only if no package is available for your distro.

1. Install `greetd` (in most distro's standard repositories) and `quickshell`

2. Create the greeter user (if not already created by greetd):
```bash
sudo groupadd -r greeter
sudo useradd -r -g greeter -d /var/lib/greeter -s /bin/bash -c "System Greeter" greeter
sudo mkdir -p /var/lib/greeter
sudo chown greeter:greeter /var/lib/greeter
```

3. Clone the hgs project to `/etc/xdg/quickshell/hgs-greeter`:
```bash
sudo git clone https://github.com/CoastLineSec/HyprGlassShell.git /etc/xdg/quickshell/hgs-greeter
```

4. Copy `Modules/Greetd/assets/hgs-greeter` to `/usr/local/bin/hgs-greeter`:
```bash
sudo cp /etc/xdg/quickshell/hgs-greeter/Modules/Greetd/assets/hgs-greeter /usr/local/bin/hgs-greeter
sudo chmod +x /usr/local/bin/hgs-greeter
```

5. Create greeter cache directory with proper permissions:
```bash
sudo mkdir -p /var/cache/hgs-greeter
sudo chown <greeter-user>:<greeter-group> /var/cache/hgs-greeter
sudo chmod 2770 /var/cache/hgs-greeter
```

6. Edit or create `/etc/greetd/config.toml`:
```toml
[terminal]
vt = 1

[default_session]
user = "greeter"
command = "/usr/local/bin/hgs-greeter --command hyprland"
```

7. Disable existing display manager and enable greetd:
```bash
sudo systemctl disable gdm sddm lightdm
sudo systemctl enable greetd
```

8. (Optional) Set up theme syncing using the manual ACL method described in the Configuration → Personalization section below

#### Legacy installation (deprecated)

If you prefer the old method with separate shell scripts and config files:
1. Copy `assets/hgs-hypr.lua` (legacy: `assets/hgs-hypr.conf`) to `/etc/greetd`
2. Copy `assets/greet-hyprland.sh` to `/usr/local/bin/start-hgs-greetd.sh`
3. Edit the config file and replace `_HGS_PATH_` with your HGS installation path
4. Configure greetd to use `/usr/local/bin/start-hgs-greetd.sh`

### NixOS

To install the greeter on NixOS add the repo to your flake inputs as described in the readme. Then somewhere in your NixOS config add this to imports:
```nix
imports = [
  inputs.hypr-glass-shell.nixosModules.greeter
]
```

Enable the greeter with this in your NixOS config:
```nix
programs.hypr-glass-shell.greeter = {
  enable = true;
  compositor.name = "hyprland";
  configHome = "/home/user"; # optionally copyies that users HGS settings (and wallpaper if set) to the greeters data directory as root before greeter starts
};
```

## Usage

### Using hgs-greeter wrapper (recommended)

The `hgs-greeter` wrapper launches the greeter under Hyprland:

```bash
hgs-greeter --command hyprland
hgs-greeter --command hyprland -C /path/to/custom-hyprland.lua
hgs-greeter --command hyprland --remember-last-user false --remember-last-session false
```

Configure greetd to use it in `/etc/greetd/config.toml`:
```toml
[terminal]
vt = 1

[default_session]
user = "greeter"
command = "/usr/bin/hgs-greeter --command hyprland"
```

### Manual usage

To run hgs in greeter mode you can also manually set environment variables:

```bash
HGS_RUN_GREETER=1 qs -p /path/to/hgs
```

### Configuration

#### Compositor

For current wrapper-based installs, the `hgs-greeter` wrapper supports Hyprland.

Wrapper-based installs use the generated Hyprland greeter config by default. If you need a custom compositor config, add `-C /path/to/config` to the `hgs-greeter` command in `/etc/greetd/config.toml`.

#### Personalization

The greeter can be personalized with wallpapers, themes, weather, clock formats, and more - configured exactly the same as hgs.

**Easiest method (single user):** Run `hgs greeter sync` to automatically sync your HGS theme with the greeter.

**Multi-user systems:** One **main admin** runs full sync once to set up greetd and the shared cache (`hgs greeter sync`, or `hgs greeter sync --local` when developing from a checkout). **Every other account**—including other admins—should only run:

```bash
hgs greeter sync --profile
```

Before that, an administrator must add each user to the `greeter` group in **Settings → Users** (greeter toggle) or with `sudo usermod -aG greeter <username>`. Each added user must log out and back in before `--profile` will work.

Per-user settings are stored under `/var/cache/hgs-greeter/users/<username>/` for the login picker; the root cache remains the default fallback and is owned by whoever ran full sync.

**Manual method:** You can manually synchronize configurations if you want greeter settings to always mirror your shell:

```bash
# Add yourself to the greeter group
sudo usermod -aG greeter $USER

# Set ACLs to allow greeter user to traverse your home directory
setfacl -m u:greeter:x ~ ~/.config ~/.local ~/.cache ~/.local/state

# Set group permissions on HGS directories
sudo chgrp -R greeter ~/.config/HyprGlassShell ~/.local/state/HyprGlassShell ~/.cache/quickshell
sudo chmod -R g+rX ~/.config/HyprGlassShell ~/.local/state/HyprGlassShell ~/.cache/quickshell

# Create symlinks for theme files
sudo ln -sf ~/.config/HyprGlassShell/settings.json /var/cache/hgs-greeter/settings.json
sudo ln -sf ~/.local/state/HyprGlassShell/session.json /var/cache/hgs-greeter/session.json
sudo ln -sf ~/.cache/HyprGlassShell/hgs-colors.json /var/cache/hgs-greeter/colors.json

# Logout and login for group membership to take effect
```

**Advanced:** You can override the configuration path with the `HGS_GREET_CFG_DIR` environment variable or the `--cache-dir` flag when using `hgs-greeter`. The default is `/var/cache/hgs-greeter`.

The cache directory should be owned by `<greeter-user>:<greeter-group>` with `2770` permissions. If the greeter user is not available yet, HGS falls back to `root:<greeter-group>`.
