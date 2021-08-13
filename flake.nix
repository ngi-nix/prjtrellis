{
  description = ''
    Project Trellis enables a fully open-source flow for ECP5 FPGAs using Yosys
    for Verilog synthesis and nextpnr for place and route. Project Trellis itself
    provides the device database and tools for bitstream creation.
  '';

  # Nixpkgs / NixOS version to use.
  inputs.nixpkgs.url = "github:NixOS/nixpkgs?ref=nixos-21.05";
  inputs.database.url = "github:YosysHQ/prjtrellis-db";
  inputs.database.flake = false;

  outputs = { self, nixpkgs, database }:
    let

      # Generate a user-friendly version numer.
      version = builtins.substring 0 8 self.lastModifiedDate;

      # System types to support.
      supportedSystems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];

      # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Nixpkgs instantiated for supported system types.
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; overlays = [ self.overlay ]; });

      prjtrellis =
        { lib, stdenv, cmake, ninja, python3, boost }:
        stdenv.mkDerivation {
          name = "prjtrellis-${version}";

          src = ./.;

          prePatch = ''
            ln -s ${database} ./database
            cd libtrellis
          '';

          buildInputs = [ boost python3 ];
          nativeBuildInputs = [ cmake ];

          meta = with lib; {
            description = "Documenting the Lattice ECP5 bit-stream format.";
            longDescription = ''
              Project Trellis enables a fully open-source flow for ECP5 FPGAs using Yosys
              for Verilog synthesis and nextpnr for place and route. Project Trellis itself
              provides the device database and tools for bitstream creation.
            '';
            homepage    = "https://github.com/YosysHQ/prjtrellis";
            license     = licenses.isc;
            platforms   = platforms.all;
          };
        };

    in

    rec {
      overlay = final: prev: {
        prjtrellis = final.callPackage prjtrellis {};
      };

      packages = forAllSystems (system:
        {
          inherit (nixpkgsFor.${system}) prjtrellis;
        });

      defaultPackage = forAllSystems (system: self.packages.${system}.prjtrellis);
      devShell = defaultPackage;

      hydraJobs.prjtrellis = self.defaultPackage;
    };
}
