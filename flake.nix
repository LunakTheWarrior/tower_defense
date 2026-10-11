{
  description = "BASTION: Linux x86_64 / GCC 16 / Bazel development toolchain";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/6510408d147d8c5e6d76043e899e9251756f6a75";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      lib = pkgs.lib;
      compiler = pkgs.gcc16;
      binutils = compiler.bintools;
      graphics = with pkgs; [
        libGL
        libx11
        libxrandr
        libxinerama
        libxcursor
        libxi
        libxext
        libxfixes
        libxrender
      ];
      includeDirectories = map (pkg: "${lib.getDev pkg}/include") (graphics ++ [ pkgs.xorgproto ]);
      libraryDirectories = map (pkg: "${lib.getLib pkg}/lib") graphics;
      runtimeDirectories = libraryDirectories ++ [ "${lib.getLib compiler.cc}/lib" ];
      manifest = pkgs.writeText "bastion-gcc16-toolchain.json" (builtins.toJSON {
        schema_version = 1;
        compiler_version = compiler.version;
        target = "x86_64-linux-gnu";
        tools = {
          gcc = "${compiler}/bin/gcc";
          cpp = "${compiler}/bin/cpp";
          gcov = "${compiler.cc}/bin/gcov";
          ar = "${binutils}/bin/ar";
          ld = "${binutils}/bin/ld";
          nm = "${binutils}/bin/nm";
          objcopy = "${binutils}/bin/objcopy";
          objdump = "${binutils}/bin/objdump";
          strip = "${binutils}/bin/strip";
          dwp = "${binutils}/bin/dwp";
        };
        include_directories = includeDirectories;
        compile_flags = lib.concatMap (dir: [ "-isystem" dir ]) includeDirectories;
        link_flags = (map (dir: "-L${dir}") libraryDirectories)
          ++ (map (dir: "-Wl,-rpath,${dir}") runtimeDirectories);
      });
      shellArgs = {
        packages = [ compiler binutils pkgs.bazel_8 pkgs.pkg-config pkgs.xorgproto ] ++ graphics;
        BASTION_GCC_TOOLCHAIN = manifest;
        # Editor/build helpers use the same immutable compiler as Bazel.
        CC = "${compiler}/bin/gcc";
        CXX = "${compiler}/bin/g++";
      };
    in
    {
      packages.${system}.toolchain-manifest = manifest;
      devShells.${system} = {
        default = pkgs.mkShellNoCC shellArgs;
        # Portable CPU rendering for non-NixOS desktops and headless CI, with no
        # dependency on the host's Mesa/NVIDIA userspace driver version.
        software = pkgs.mkShellNoCC (shellArgs // {
          packages = shellArgs.packages ++ [ pkgs.mesa pkgs.xvfb-run ];
          LIBGL_ALWAYS_SOFTWARE = "1";
          LIBGL_DRIVERS_PATH = "${pkgs.mesa}/lib/dri";
          __GLX_VENDOR_LIBRARY_NAME = "mesa";
          LD_LIBRARY_PATH = lib.makeLibraryPath [ pkgs.mesa ];
        });
      };
    };
}
