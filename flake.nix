{
  description = "86Box - x86 machine emulator with Voodoo tracing";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          package = pkgs.stdenv.mkDerivation (finalAttrs: {
            pname = "86box";
            version = "5.0";

            src = self;

            postPatch = ''
              substituteAllInPlace src/qt/qt_platform.cpp
            '';

            nativeBuildInputs = with pkgs; [
              cmake
              pkg-config
              makeWrapper
              libsForQt5.wrapQtAppsHook
              extra-cmake-modules
              wayland-scanner
            ];

            buildInputs = with pkgs; [
              freetype
              fluidsynth
              SDL2
              glib
              openal
              rtmidi
              pcre2
              jack2
              libpcap
              libslirp
              libsForQt5.qtbase
              libsForQt5.qttools
              libsndfile
              flac.dev
              libogg.dev
              libvorbis.dev
              libopus.dev
              libmpg123.dev
              libmt32emu
              alsa-lib
              wayland
            ];

            cmakeFlags = [
              "-DUSE_QT5=ON"
            ];

            postInstall = ''
              install -Dm644 -t $out/share/applications $src/src/unix/assets/net.86box.86Box.desktop

              for size in 48 64 72 96 128 192 256 512; do
                install -Dm644 -t $out/share/icons/hicolor/"$size"x"$size"/apps \
                  $src/src/unix/assets/"$size"x"$size"/net.86box.86Box.png
              done;

              mkdir -p $out/share/86Box
              ln -s ${finalAttrs.passthru.roms} $out/share/86Box/roms
            '';

            passthru = {
              roms = pkgs.fetchFromGitHub {
                owner = "86Box";
                repo = "roms";
                rev = "v${finalAttrs.version}";
                hash = "sha256-bMCmDAdGTkO3BuU0EBC1svulZYP3tPqWBELbXwV0KO8=";
              };
            };

            preFixup = ''
              makeWrapperArgs+=(--prefix LD_LIBRARY_PATH : "${pkgs.lib.makeLibraryPath [ pkgs.libpcap ]}")
            '';

            meta = with pkgs.lib; {
              description = "Emulator of x86-based machines with Voodoo tracing support";
              mainProgram = "86Box";
              homepage = "https://86box.net/";
              license = licenses.gpl2Plus;
              platforms = platforms.linux;
            };
          });
        in
        {
          default = package;
          "86box" = package;
        }
      );
    };
}
