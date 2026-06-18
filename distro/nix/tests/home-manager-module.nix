{
  self,
  pkgs,
  ...
}:
let
  homeManagerNixosModule =
    (fetchTarball {
      url = "https://github.com/nix-community/home-manager/archive/e82d4a4ecd18363aa2054cbaa3e32e4134c3dbf4.tar.gz";
      sha256 = "sha256-ZTYDofOM3/PJhRF1EuBh6uibm+DmkhU7Wor6mMN7YTc=";
    })
    + "/nixos";
in
pkgs.testers.runNixOSTest {
  name = "hgs-home-manager-module";

  nodes.machine = {
    ...
  }: {
    imports = [
      homeManagerNixosModule
    ];

    users.users.coastlinesec = {
      isNormalUser = true;
      createHome = true;
      home = "/home/coastlinesec";
      extraGroups = [ "wheel" ];
    };

    home-manager.useGlobalPkgs = true;
    home-manager.useUserPackages = true;

    home-manager.users.coastlinesec = {
      pkgs,
      ...
    }: {
      imports = [
        self.homeModules.hypr-glass-shell
      ];

      home.username = "coastlinesec";
      home.homeDirectory = "/home/coastlinesec";
      home.stateVersion = "25.11";

      programs.hypr-glass-shell = {
        enable = true;
        systemd = {
          enable = true;
          target = "default.target";
        };

        settings = {
          theme = "integration-test";
        };

        clipboardSettings = {
          maxItems = 10;
        };

        session = {
          startedFrom = "nixos-test";
        };

        plugins.TestPlugin = {
          enable = true;
          src = pkgs.runCommand "hgs-test-plugin" { } ''
            mkdir -p "$out"
            echo plugin > "$out/plugin.txt"
          '';
          settings = {
            enabled = true;
            source = "test";
          };
        };
      };
    };

    system.stateVersion = "25.11";
  };

  testScript = ''
    import json

    machine.wait_for_unit("multi-user.target")

    machine.succeed("su -- coastlinesec -c 'command -v hgs'")
    machine.succeed("su -- coastlinesec -c 'test -f ~/.config/HyprGlassShell/settings.json'")
    machine.succeed("su -- coastlinesec -c 'test -f ~/.config/HyprGlassShell/clsettings.json'")
    machine.succeed("su -- coastlinesec -c 'test -f ~/.config/HyprGlassShell/plugin_settings.json'")
    machine.succeed("su -- coastlinesec -c 'test -e ~/.config/HyprGlassShell/plugins/TestPlugin'")
    machine.succeed("su -- coastlinesec -c 'test -f ~/.local/state/HyprGlassShell/session.json'")

    settings = json.loads(machine.succeed("su -- coastlinesec -c 'cat ~/.config/HyprGlassShell/settings.json'"))
    clipboard = json.loads(machine.succeed("su -- coastlinesec -c 'cat ~/.config/HyprGlassShell/clsettings.json'"))
    session = json.loads(machine.succeed("su -- coastlinesec -c 'cat ~/.local/state/HyprGlassShell/session.json'"))
    plugins = json.loads(machine.succeed("su -- coastlinesec -c 'cat ~/.config/HyprGlassShell/plugin_settings.json'"))
    doctor = json.loads(machine.succeed("su -- coastlinesec -c 'hgs doctor --json'"))

    t.assertEqual(settings["theme"], "integration-test")
    t.assertEqual(clipboard["maxItems"], 10)
    t.assertEqual(session["startedFrom"], "nixos-test")
    t.assertTrue(plugins["TestPlugin"]["enabled"])
    t.assertEqual(plugins["TestPlugin"]["source"], "test")
    t.assertIsInstance(doctor.get("results"), list)
  '';
}
