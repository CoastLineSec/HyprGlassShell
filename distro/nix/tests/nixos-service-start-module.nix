{
  self,
  pkgs,
  ...
}:
let
  fakeHgs = pkgs.writeShellScriptBin "hgs" ''
    printf '%s\n' "$@" > /tmp/hgs-service-args
    exec ${pkgs.coreutils}/bin/sleep 300
  '';
in
pkgs.testers.runNixOSTest {
  name = "hgs-nixos-service-start-module";

  nodes.machine = {
    imports = [
      self.nixosModules.hypr-glass-shell
    ];

    users.users.coastlinesec = {
      isNormalUser = true;
      linger = true;
      extraGroups = [ "wheel" ];
    };

    programs.hypr-glass-shell = {
      enable = true;
      package = fakeHgs;
      systemd = {
        enable = true;
        target = "default.target";
      };
    };

    system.stateVersion = "25.11";
  };

  testScript = ''
    machine.wait_for_unit("multi-user.target")
    machine.wait_for_unit("user@1000.service")

    machine.succeed("systemctl --machine=coastlinesec@ --user start hgs.service")
    machine.wait_until_succeeds("systemctl --machine=coastlinesec@ --user is-active hgs.service")
    machine.wait_until_succeeds("test -f /tmp/hgs-service-args")
    machine.succeed("grep -Fx run /tmp/hgs-service-args")
    machine.succeed("grep -Fx -- --session /tmp/hgs-service-args")
  '';
}
