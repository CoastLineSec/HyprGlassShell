{
  lib,
  hgsPkgs,
  pkgs,
  ...
}:
let
  inherit (lib) types;
  path = [
    "programs"
    "hypr-glass-shell"
  ];
  jsonFormat = pkgs.formats.json { };
  builtInRemovedMsg = "This is now built-in in HGS and doesn't need additional dependencies.";
in
{
  imports = [
    (lib.mkRemovedOptionModule (path ++ [ "enableBrightnessControl" ]) builtInRemovedMsg)
    (lib.mkRemovedOptionModule (path ++ [ "enableColorPicker" ]) builtInRemovedMsg)
    (lib.mkRemovedOptionModule (path ++ [ "enableClipboard" ]) builtInRemovedMsg)
    (lib.mkRemovedOptionModule (
      path ++ [ "enableSystemSound" ]
    ) "qtmultimedia is now included on hgs-shell package.")
    ./hgs-rename.nix
  ];

  options.programs.hypr-glass-shell = {
    enable = lib.mkEnableOption "HyprGlassShell";
    package = lib.mkPackageOption hgsPkgs "hgs-shell" {
      extraDescription = "The HyprGlassShell package to use (defaults to be built from source)";
    };

    systemd = {
      enable = lib.mkEnableOption "HyprGlassShell systemd startup";
      restartIfChanged = lib.mkOption {
        type = types.bool;
        default = true;
        description = "Auto-restart hgs.service when hypr-glass-shell changes";
      };
    };

    dgop = {
      package = lib.mkPackageOption pkgs "dgop" { };
    };

    enableSystemMonitoring = lib.mkOption {
      type = types.bool;
      default = true;
      description = "Add needed dependencies to use system monitoring widgets";
    };

    enableVPN = lib.mkOption {
      type = types.bool;
      default = true;
      description = "Add needed dependencies to use the VPN widget";
    };

    enableDynamicTheming = lib.mkOption {
      type = types.bool;
      default = true;
      description = "Add needed dependencies to have dynamic theming support";
    };

    enableAudioWavelength = lib.mkOption {
      type = types.bool;
      default = true;
      description = "Add needed dependencies to have audio wavelength support";
    };

    enableCalendarEvents = lib.mkOption {
      type = types.bool;
      default = true;
      description = "Add calendar events support via khal";
    };

    enableClipboardPaste = lib.mkOption {
      type = types.bool;
      default = true;
      description = "Adds needed dependencies for directly pasting items from the clipboard history.";
    };

    quickshell = {
      package = lib.mkPackageOption pkgs "quickshell" {
        extraDescription = "(we recommend at least 0.3.0, currently available in nixos-unstable)";
      };
    };

    plugins = lib.mkOption {
      type = types.attrsOf (
        types.submodule {
          options = {
            enable = lib.mkOption {
              type = types.bool;
              default = true;
              description = "Whether to enable this plugin";
            };
            src = lib.mkOption {
              type = types.either types.package types.path;
              description = "Source of the plugin package or path";
            };
            settings = lib.mkOption {
              type = jsonFormat.type;
              default = { };
              description = "Plugin settings as an attribute set";
            };
          };
        }
      );
      default = { };
      description = "HGS Plugins to install and enable";
      example = lib.literalExpression ''
        {
          DockerManager = {
            src = pkgs.fetchFromGitHub {
              owner = "LuckShiba";
              repo = "HgsDockerManager";
              rev = "v1.2.0";
              sha256 = "sha256-VoJCaygWnKpv0s0pqTOmzZnPM922qPDMHk4EPcgVnaU=";
            };
          };
          AnotherPlugin = {
            enable = true;
            src = pkgs.another-plugin;
          };
        }
      '';
    };
  };
}
