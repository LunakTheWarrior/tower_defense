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
        xorg.libX11
        xorg.libXrandr
        xorg.libXinerama
        xorg.libXcursor
        xorg.libXi
      ];
      includeDirectories = map (pkg: "${lib.getDev pkg}/include") graphics;
      libraryDirectories = map (pkg: "${lib.getLib pkg}/lib") graphics;
      runtimeDirectories = libraryDirectories ++ [ "${lib.getLib compiler.cc}/lib" ];
      manifest = pkgs.writeText "bastion-gcc16-toolchain.json" (builtins.toJSON {
        schema_version = 1;
        compiler_version = compiler.version;
        target = "x86_64-linux-gnu";
        tools = {
          gcc = "${compiler}/bin/gcc";
          cpp = "${compiler}/bin/cpp";
          gcov = "${compiler}/bin/gcov";
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
    in
    {
      packages.${system}.toolchain-manifest = manifest;
      devShells.${system}.default = pkgs.mkShellNoCC {
        packages = [ compiler binutils pkgs.bazel_8 pkgs.pkg-config ] ++ graphics;
        BASTION_GCC_TOOLCHAIN = manifest;
        # Native Bazel compiler discovery is disabled by our registered toolchain;
        # these also keep editor/build helpers on the same immutable compiler.
        CC = "${compiler}/bin/gcc";
        CXX = "${compiler}/bin/g++";
      };
    };
}
