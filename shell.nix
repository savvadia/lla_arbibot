{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    (readline.overrideAttrs (oldAttrs: {
      meta.platforms = oldAttrs.meta.platforms ++ [ "x86_64-darwin" ];
    }))
    cmake
    jsoncpp
  ];
}
