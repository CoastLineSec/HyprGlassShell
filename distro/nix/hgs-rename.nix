{ lib, ... }:
{
  imports = [
    (lib.mkRenamedOptionModule
      [
        "programs"
        "hgsMaterialShell"
      ]
      [
        "programs"
        "hypr-glass-shell"
      ]
    )
  ];
}
