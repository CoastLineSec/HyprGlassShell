{
  config,
  pkgs,
  lib,
  ...
}@args:
let
  cfg = config.programs.hypr-glass-shell;
  jsonFormat = pkgs.formats.json { };
  common = import ./common.nix {
    inherit
      config
      pkgs
      lib
      ;
  };
  hasPluginSettings = lib.any (plugin: plugin.settings != { }) (
    lib.attrValues (lib.filterAttrs (n: v: v.enable) cfg.plugins)
  );
  pluginSettings = lib.mapAttrs (name: plugin: { enabled = plugin.enable; } // plugin.settings) (
    lib.filterAttrs (n: v: v.enable) cfg.plugins
  );
in
{
  imports = [
    (import ./options.nix args)
    (lib.mkRemovedOptionModule [
      "programs"
      "hypr-glass-shell"
      "enableNightMode"
    ] "Night mode is now always available")
    (lib.mkRemovedOptionModule [
      "programs"
      "hypr-glass-shell"
      "default"
      "settings"
    ] "Default settings have been removed and been replaced with programs.hypr-glass-shell.settings")
    (lib.mkRemovedOptionModule [
      "programs"
      "hypr-glass-shell"
      "default"
      "session"
    ] "Default session has been removed and been replaced with programs.hypr-glass-shell.session")
    (lib.mkRenamedOptionModule
      [ "programs" "hypr-glass-shell" "enableSystemd" ]
      [ "programs" "hypr-glass-shell" "systemd" "enable" ]
    )
  ];

  options.programs.hypr-glass-shell = {
    settings = lib.mkOption {
      type = jsonFormat.type;
      default = { };
      description = "HyprGlassShell configuration settings as an attribute set, to be written to ~/.config/HyprGlassShell/settings.json.";
    };

    clipboardSettings = lib.mkOption {
      type = jsonFormat.type;
      default = { };
      description = "HyprGlassShell clipboard settings as an attribute set, to be written to ~/.config/HyprGlassShell/clsettings.json.";
    };

    session = lib.mkOption {
      type = jsonFormat.type;
      default = { };
      description = "HyprGlassShell session settings as an attribute set, to be written to ~/.local/state/HyprGlassShell/session.json.";
    };

    managePluginSettings = lib.mkOption {
      type = lib.types.bool;
      default = hasPluginSettings;
      description = ''Whether to manage plugin settings. Automatically enabled if any plugins have settings configured.'';
    };

    systemd.target = lib.mkOption {
      type = lib.types.str;
      default = config.wayland.systemd.target;
      defaultText = lib.literalExpression "config.wayland.systemd.target";
      description = "Systemd target to bind to.";
    };
  };

  config = lib.mkIf cfg.enable {
    programs.quickshell = {
      enable = true;
      inherit (cfg.quickshell) package;
    };

    systemd.user.services.hgs = lib.mkIf cfg.systemd.enable {
      Unit = {
        Description = "HyprGlassShell";
        PartOf = [ cfg.systemd.target ];
        After = [ cfg.systemd.target ];
      };

      Service = {
        ExecStart = lib.getExe cfg.package + " run --session";
        Restart = "on-failure";
      };

      Install.WantedBy = [ cfg.systemd.target ];
    };

    xdg.stateFile."HyprGlassShell/session.json" = lib.mkIf (cfg.session != { }) {
      source = jsonFormat.generate "session.json" cfg.session;
    };

    xdg.configFile = {
      "HyprGlassShell/settings.json" = lib.mkIf (cfg.settings != { }) {
        source = jsonFormat.generate "settings.json" cfg.settings;
      };
      "HyprGlassShell/clsettings.json" = lib.mkIf (cfg.clipboardSettings != { }) {
        source = jsonFormat.generate "clsettings.json" cfg.clipboardSettings;
      };
      "HyprGlassShell/plugin_settings.json" = lib.mkIf cfg.managePluginSettings {
        source = jsonFormat.generate "plugin_settings.json" pluginSettings;
      };
    }
    // (lib.mapAttrs' (name: value: {
      name = "HyprGlassShell/plugins/${name}";
      inherit value;
    }) common.plugins);
    warnings =
      lib.optional (!cfg.managePluginSettings && hasPluginSettings)
        "You have disabled managePluginSettings but provided plugin settings. These settings will be ignored.";
    home.packages = common.packages;
  };
}
