{
  self,
  pkgs,
  ...
}:
pkgs.testers.runNixOSTest {
  name = "hgs-nixos-module";

  nodes.machine = {
    imports = [
      self.nixosModules.hypr-glass-shell
    ];

    users.users.coastlinesec = {
      isNormalUser = true;
      extraGroups = [ "wheel" ];
    };

    programs.hypr-glass-shell = {
      enable = true;
      systemd.enable = true;
      plugins = {
        TestPlugin = {
          src = pkgs.emptyDirectory;
        };
      };
    };

    system.stateVersion = "25.11";
  };

  testScript = ''
    import json

    machine.wait_for_unit("multi-user.target")

    machine.succeed("command -v hgs")
    machine.succeed("command -v quickshell")
    machine.succeed("su -- coastlinesec -c 'hgs --help >/dev/null'")
    machine.succeed("test -d /etc/xdg/quickshell/hgs-plugins")
    machine.succeed("test -f /run/current-system/sw/lib/systemd/user/hgs.service")

    payload = json.loads(machine.succeed("su -- coastlinesec -c 'hgs doctor --json'"))
    t.assertIn("summary", payload)
    t.assertIsInstance(payload.get("results"), list)
  '';
}
