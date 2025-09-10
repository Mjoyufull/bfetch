{
  description = "bfetch - The fastest system information fetch tool in the universe";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachSystem [ "x86_64-linux" ] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = {
          default = pkgs.stdenv.mkDerivation {
            pname = "bfetch";
            version = "1.1.2-cowmeed";

            src = ./.;

            nativeBuildInputs = with pkgs; [
              gcc
              gnumake
            ];

            buildPhase = ''
              make fast
            '';

            installPhase = ''
              mkdir -p $out/bin
              cp bfetch $out/bin/
            '';

            meta = with pkgs.lib; {
              description = "Ultra-fast system information fetch tool with Gentoo support";
              longDescription = ''
                bfetch is a lightning-fast system information display tool written in C.
                It's optimized for Bedrock Linux and other systems, running 55x faster
                than fastfetch with comprehensive package manager support.
                
                Features:
                - Beautiful ASCII art with Nord color scheme (Bedrock + Gentoo modes)
                - 0.002 second execution time (55x faster than fastfetch)
                - Direct filesystem access for all package managers
                - Perfect GPU detection (NVIDIA GeForce RTX format)
                - Fixed terminal detection (shows actual terminal, not shell)
                - --gentoo flag for Gentoo ASCII art
                - Support for RPM, DPKG, Pacman, Emerge, Nix packages
                - Aggressive compiler optimizations
              '';
              homepage = "https://github.com/Mjoyufull/bfetch";
              license = licenses.mit;
              platforms = platforms.linux;
              maintainers = [ ];
            };
          };

          bfetch = self.packages.${system}.default;
        };

        devShells = {
          default = pkgs.mkShell {
            buildInputs = with pkgs; [
              gcc
              gnumake
              gdb
              valgrind
            ];

            shellHook = ''
              echo "bfetch development environment loaded!"
              echo "Build with: make fast"
              echo "Run with: ./bfetch"
            '';
          };
        };

        apps = {
          default = {
            type = "app";
            program = "${self.packages.${system}.default}/bin/bfetch";
            meta = {
              description = "Ultra-fast system information fetch tool";
              platforms = pkgs.lib.platforms.linux;
            };
          };
        };
      });
}
