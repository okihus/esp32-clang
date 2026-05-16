{
  pkgs,
  inputs,
  ...
}: let
  esp-idf = inputs.nixpkgs-esp-dev.packages.${pkgs.stdenv.system}.esp-idf-riscv;
in {
  packages = [
    esp-idf
    pkgs.espflash
    pkgs.tio
    pkgs.just
  ];

  enterShell = ''
    echo "ESP-IDF ready for ESP32-C6. Try: just dev"
  '';
}
