{
  description = "JS8Call is software using the JS8 Digital Mode providing weak signal keyboard to keyboard messaging to Amateur Radio Operators.";
  outputs = { self, nixpkgs }: {
    defaultPackage.x86_64-linux = self.packages.x86_64-linux.js8call;
    devShells.x86_64-linux.default = let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
    in pkgs.mkShell {
      buildInputs = with pkgs; [
        pkg-config
        hamlib
        libusb1
        cmake
        gfortran
        fftw fftwFloat
      ] ++ (with pkgs.qt5; [
        qtbase
        qtmultimedia
        qtserialport
      ]);
    };
    packages.x86_64-linux.js8call = let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
    in pkgs.stdenv.mkDerivation {
      pname = "js8call";
      version = "2.2.0";
      src = ./.;
      patchPhase = ''
        substituteInPlace CMakeLists.txt \
            --replace "/usr/share/applications" "$out/share/applications" \
            --replace "/usr/share/pixmaps" "$out/share/pixmaps" \
            --replace "/usr/bin/" "$out/bin"
      '';
      nativeBuildInputs = with pkgs.qt5; [ wrapQtAppsHook ];
      buildInputs = with pkgs; [
        pkg-config
        hamlib
        libusb1
        cmake
        gfortran
        fftw fftwFloat
      ] ++ (with pkgs.qt5; [
        qtbase
        qtmultimedia
        qtserialport
      ]);
    };
  };
}
