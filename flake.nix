{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs";
    crane.url = "github:ipetkov/crane";
  };

  outputs =
    {
      nixpkgs,
      crane,
      ...
    }:
    let
      forAllSystems =
        function:
        nixpkgs.lib.genAttrs
          [
            "x86_64-linux"
          ]
          (
            system:
            let
              pkgs = import nixpkgs { inherit system; };

              depsBuildBuild = with pkgs; [ pkg-config ];
              nativeBuildInputs = with pkgs; [
                meson
                ninja
                pkg-config
                wayland-scanner

              ];

              buildInputs = with pkgs; [
                gtk3
                libxkbcommon
                opencv
                pixman
                wayland
                wayland-protocols
              ];

            in
            function {
              inherit
                pkgs
                buildInputs
                nativeBuildInputs
                depsBuildBuild
                ;

            }
          );
    in
    {

      packages = forAllSystems (
        {
          pkgs,
          nativeBuildInputs,
          buildInputs,
          depsBuildBuild,
        }:
        {
          default = pkgs.stdenv.mkDerivation {
            name = "wl-kbptr";
            inherit buildInputs nativeBuildInputs depsBuildBuild;
            src = ./.;

            mesonFlags = [
              "-Dopencv=enabled"
            ];

            strictDeps = true;

          };
        }
      );

      templates.default.path = ./.;

      devShells = forAllSystems (
        {
          pkgs,
          buildInputs,
          nativeBuildInputs,
          depsBuildBuild,
        }:
        {
          default =

            pkgs.mkShell {
              inherit nativeBuildInputs buildInputs depsBuildBuild;
              packages = with pkgs; [
                lldb

                compiledb
                makeWrapper
                autotools-language-server
              ];

            };
        }
      );

    };
}
