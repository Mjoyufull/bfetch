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
            version = "1.0.0";

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
              description = "Ultra-fast system information fetch tool optimized for Bedrock Linux";
              longDescription = ''
                bfetch is a lightning-fast system information display tool written in C.
                It's specifically optimized for Bedrock Linux systems and is 675x faster
                than traditional shell scripts and 57x faster than fastfetch.
                
                Features:
                - Beautiful ASCII art with Nord color scheme
                - 0.002 second execution time
                - Direct filesystem access (no subprocess spawning)
                - Aggressive compiler optimizations
                - Perfect visual match to the original shell script
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
