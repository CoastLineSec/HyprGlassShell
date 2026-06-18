{
  description = "Hypr Glass Shell";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-compat = {
      url = "github:NixOS/flake-compat";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      ...
    }:
    let
      goModVersion =
        let
          content = builtins.readFile ./core/go.mod;
          lines = builtins.filter builtins.isString (builtins.split "\n" content);
          goLines = builtins.filter (l: builtins.match "go [0-9]+\\..*" l != null) lines;
          matched =
            if goLines != [ ] then builtins.match "go ([0-9]+)\\.([0-9]+).*" (builtins.head goLines) else null;
        in
        if matched != null then
          {
            major = builtins.elemAt matched 0;
            minor = builtins.elemAt matched 1;
          }
        else
          {
            major = "1";
            minor = "25";
          };
      goForPkgs = pkgs: pkgs.${"go_${goModVersion.major}_${goModVersion.minor}"};
      forEachSystem =
        fn:
        nixpkgs.lib.genAttrs [ "aarch64-darwin" "aarch64-linux" "x86_64-darwin" "x86_64-linux" ] (
          system: fn system nixpkgs.legacyPackages.${system}
        );
      forEachLinuxSystem =
        fn:
        nixpkgs.lib.genAttrs [ "aarch64-linux" "x86_64-linux" ] (
          system: fn system nixpkgs.legacyPackages.${system}
        );

      mkModuleWithHgsPkgs =
        modulePath:
        args@{ pkgs, ... }:
        {
          imports = [
            (import modulePath (args // { hgsPkgs = buildHgsPkgs pkgs; }))
          ];
        };

      mkQmlImportPath =
        pkgs: qmlPkgs:
        pkgs.lib.concatStringsSep ":" (map (o: "${o}/${pkgs.qt6.qtbase.qtQmlPrefix}") qmlPkgs);

      mkQtPluginPath =
        pkgs: qtPkgs:
        pkgs.lib.concatStringsSep ":" (map (o: "${o}/${pkgs.qt6.qtbase.qtPluginPrefix}") qtPkgs);

      qmlPkgs =
        pkgs: with pkgs.kdePackages; [
          kirigami.unwrapped
          sonnet
          qtmultimedia
          qtimageformats
          kimageformats
        ];

      # Allows downstream modules to provide their own 'pkgs' (with overlays)
      # instead of being forced to use the flake's locked nixpkgs.
      mkHgsShell =
        pkgs:
        let
          mkDate =
            longDate:
            pkgs.lib.concatStringsSep "-" [
              (builtins.substring 0 4 longDate)
              (builtins.substring 4 2 longDate)
              (builtins.substring 6 2 longDate)
            ];
          version =
            let
              rawVersion = pkgs.lib.removePrefix "v" (pkgs.lib.trim (builtins.readFile ./quickshell/VERSION));
              cleanVersion = builtins.replaceStrings [ " " ] [ "" ] rawVersion;
              dateSuffix = "+date=" + mkDate (self.lastModifiedDate or "19700101");
              revSuffix = "_" + (self.shortRev or "dirty");
            in
            "${cleanVersion}${dateSuffix}${revSuffix}";
        in
        pkgs.lib.makeOverridable (
          {
            extraQtPackages ? [ ],
          }:
          (pkgs.buildGoModule.override { go = goForPkgs pkgs; }) (
            let
              rootSrc = ./.;
              qtPackages = (qmlPkgs pkgs) ++ extraQtPackages;
            in
            {
              inherit version;
              pname = "hgs-shell";
              src = ./core;
              vendorHash = "sha256-nvxFHQhOfBGl3h51fgYDb39K0NCj+H8mAEyKr1qOwJQ=";

              subPackages = [ "cmd/hgs" ];

              ldflags = [
                "-s"
                "-w"
                "-X 'main.Version=${version}'"
              ];

              nativeBuildInputs = with pkgs; [
                installShellFiles
                makeWrapper
              ];

              postInstall = ''
                mkdir -p $out/share/quickshell/hgs
                cp -r ${rootSrc}/quickshell/. $out/share/quickshell/hgs/

                chmod u+w $out/share/quickshell/hgs/VERSION
                echo "${version}" > $out/share/quickshell/hgs/VERSION

                # Install desktop file and icon
                install -D ${rootSrc}/assets/hgs-open.desktop \
                  $out/share/applications/hgs-open.desktop
                install -D ${rootSrc}/core/assets/hgslogo.svg \
                  $out/share/hicolor/scalable/apps/hgslogo.svg

                wrapProgram $out/bin/hgs \
                  --add-flags "-c $out/share/quickshell/hgs" \
                  --prefix "NIXPKGS_QT6_QML_IMPORT_PATH" ":" "${mkQmlImportPath pkgs qtPackages}" \
                  --prefix "QT_PLUGIN_PATH" ":" "${mkQtPluginPath pkgs qtPackages}"

                install -Dm644 ${rootSrc}/assets/systemd/hgs.service \
                  $out/lib/systemd/user/hgs.service

                substituteInPlace $out/lib/systemd/user/hgs.service \
                  --replace-fail /usr/bin/hgs $out/bin/hgs \
                  --replace-fail /usr/bin/pkill ${pkgs.procps}/bin/pkill

                substituteInPlace $out/share/quickshell/hgs/Modules/Greetd/assets/hgs-greeter \
                  --replace-fail /bin/bash ${pkgs.bashInteractive}/bin/bash

                substituteInPlace $out/share/quickshell/hgs/assets/pam/fprint \
                  --replace-fail pam_fprintd.so ${pkgs.fprintd}/lib/security/pam_fprintd.so

                substituteInPlace $out/share/quickshell/hgs/assets/pam/u2f \
                  --replace-fail pam_u2f.so ${pkgs.pam_u2f}/lib/security/pam_u2f.so

                installShellCompletion --cmd hgs \
                  --bash <($out/bin/hgs completion bash) \
                  --fish <($out/bin/hgs completion fish) \
                  --zsh <($out/bin/hgs completion zsh)
              '';

              meta = {
                description = "Hyprland-first desktop shell built with Quickshell and Go";
                homepage = "https://coastlinesec.com";
                changelog = "https://github.com/CoastLineSec/HyprGlassShell/releases/tag/v${version}";
                license = pkgs.lib.licenses.mit;
                mainProgram = "hgs";
                platforms = pkgs.lib.platforms.linux;
              };
            }
          )
        ) { };

      buildHgsPkgs = pkgs: {
        hgs-shell = mkHgsShell pkgs;
      };
    in
    {
      packages = forEachSystem (
        system: pkgs: {
          hgs-shell = mkHgsShell pkgs;
          default = self.packages.${system}.hgs-shell;
          quickshell = builtins.warn "hypr-glass-shell: the package Quickshell is not included in the HGS flake anymore. We recommend you to use the one from nixos-unstable branch of Nixpkgs or the upstream flake." pkgs.quickshell;
        }
      );

      lib = { inherit mkHgsShell buildHgsPkgs; };

      homeModules.hypr-glass-shell = mkModuleWithHgsPkgs ./distro/nix/home.nix;

      homeModules.default = self.homeModules.hypr-glass-shell;

      homeModules.hgsMaterialShell.default = builtins.warn "hypr-glass-shell: flake output `homeModules.hgsMaterialShell.default` has been renamed to `homeModules.hypr-glass-shell`" self.homeModules.hypr-glass-shell;

      nixosModules.hypr-glass-shell = mkModuleWithHgsPkgs ./distro/nix/nixos.nix;

      nixosModules.default = self.nixosModules.hypr-glass-shell;

      nixosModules.greeter = mkModuleWithHgsPkgs ./distro/nix/greeter.nix;

      nixosModules.hgsMaterialShell = builtins.warn "hypr-glass-shell: flake output `nixosModules.hgsMaterialShell` has been renamed to `nixosModules.hypr-glass-shell`" self.nixosModules.hypr-glass-shell;

      devShells = forEachSystem (
        system: pkgs:
        let
          devQmlPkgs = with pkgs;
          [
            quickshell
            kdePackages.qtdeclarative
          ]
          ++ (qmlPkgs pkgs);
        in
        {
          default = pkgs.mkShell {
            buildInputs =
              with pkgs;
              [
                (goForPkgs pkgs)
                go-mockery_2
                gopls
                delve
                go-tools
                gnumake

                prek
                uv # for prek
                shellcheck

                # Nix development tools
                nixd
                nil
              ]
              ++ devQmlPkgs;

            shellHook = ''
              touch quickshell/.qmlls.ini 2>/dev/null
              if [ ! -f .git/hooks/pre-commit ]; then prek install; fi
            '';

            QML2_IMPORT_PATH = mkQmlImportPath pkgs devQmlPkgs;
            QT_PLUGIN_PATH = mkQtPluginPath pkgs devQmlPkgs;
          };
        }
      );

      nixosTests = forEachLinuxSystem (
        system: pkgs:
        import ./distro/nix/tests {
          inherit
            self
            pkgs
            ;
          lib = pkgs.lib;
        }
      );
    };
}
