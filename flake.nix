{
	inputs = {
		nixpkgs.url = "github:nixos/nixpkgs/master";
	};
	outputs = { self, nixpkgs }:
	let
		pkgs = import nixpkgs {
			system = "x86_64-linux";
		};
	in {
		packages.x86_64-linux.default = pkgs.llvmPackages_16.libcxxStdenv.mkDerivation {
			name = "wl-kbptr";
			src = ./.;
			nativeBuildInputs = [
				pkgs.meson
				pkgs.ninja
				pkgs.pkg-config
			];
			buildInputs = [
				pkgs.wayland
				pkgs.wayland-protocols
				pkgs.cairo
				pkgs.libxkbcommon
			];
			enableParallelBuilding = true;
			mesonFlags = [];
		};
	};
}
