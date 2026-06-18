{
  config,
  pkgs,
  lib,
  ...
}@args:
let
  cfg = config.programs.hypr-glass-shell;
  common = import ./common.nix {
    inherit
      config
      pkgs
      lib
      ;
  };
in
{
  imports = [
    (import ./options.nix args)
  ];
  options.programs.hypr-glass-shell.systemd.target = lib.mkOption {
    type = lib.types.str;
    description = "Systemd target to bind to.";
    default = "graphical-session.target";
  };
  config = lib.mkIf cfg.enable {
    systemd.user.services.hgs = lib.mkIf cfg.systemd.enable {
      description = "HyprGlassShell";
      path = lib.mkForce [ ];

      partOf = [ cfg.systemd.target ];
      after = [ cfg.systemd.target ];
      wantedBy = [ cfg.systemd.target ];
      restartIfChanged = cfg.systemd.restartIfChanged;

      serviceConfig = {
        ExecStart = lib.getExe cfg.package + " run --session";
        Restart = "on-failure";
      };
    };

    environment.systemPackages = [ cfg.quickshell.package ] ++ common.packages;

    environment.etc = lib.mapAttrs' (name: value: {
      name = "xdg/quickshell/hgs-plugins/${name}";
      inherit value;
    }) common.plugins;

    services.power-profiles-daemon.enable = lib.mkDefault true;
    services.accounts-daemon.enable = lib.mkDefault true;
    services.geoclue2.enable = lib.mkDefault true;
    security.polkit.enable = lib.mkDefault true;
  };
}
