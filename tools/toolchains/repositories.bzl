"""Register native GCC by default, with an explicitly selected optional Nix SDK."""

_TOOLS = ["gcc", "cpp", "gcov", "ar", "ld", "nm", "objcopy", "objdump", "strip", "dwp"]

def _run(ctx, args):
    result = ctx.execute(
        args,
        environment = {
            # Probe the wrapper's intrinsic SDK, not incidental shell flags.
            "NIX_CFLAGS_COMPILE": "",
            "NIX_CFLAGS_LINK": "",
            "NIX_LDFLAGS": "",
            "CPATH": "",
            "C_INCLUDE_PATH": "",
            "CPLUS_INCLUDE_PATH": "",
            "LIBRARY_PATH": "",
            "LC_ALL": "C",
        },
    )
    if result.return_code:
        fail("Toolchain probe failed: {}\n{}".format(args, result.stderr))
    return result

def _materialize(ctx, manifest):
    tools = manifest["tools"]
    version = manifest["compiler_version"]
    triple = _run(ctx, [tools["gcc"], "-dumpmachine"]).stdout.strip()
    if not triple.startswith("x86_64-") or "linux" not in triple:
        fail("Compiler is not targeting x86_64 Linux: " + triple)

    # GCC reports libc, libstdc++, and compiler-internal header roots. These are
    # allowed by Bazel's include scanner without allowing an entire SDK store.
    includes = list(manifest["include_directories"])
    for language in ["c", "c++"]:
        output = _run(ctx, [tools["gcc"], "-E", "-x", language, "-v", "/dev/null"]).stderr
        reading = False
        for line in output.splitlines():
            if line == "#include <...> search starts here:":
                reading = True
            elif line == "End of search list.":
                reading = False
            elif reading:
                path = line.strip()
                for candidate in [path, str(ctx.path(path).realpath)]:
                    if candidate not in includes:
                        includes.append(candidate)

    ctx.file("probe.cc", "#include <expected>\nstatic_assert(__cplusplus >= 202100L);\nstd::expected<int, int> value{42};\n")
    _run(ctx, [tools["gcc"], "-std=c++23", "-fsyntax-only", str(ctx.path("probe.cc"))])
    ctx.file("toolchain.json", json.encode_indent(manifest))
    ctx.file("BUILD.bazel", """\
load({config}, "gcc_toolchain")
package(default_visibility = ["//visibility:public"])
gcc_toolchain(
    name = "toolchain",
    tools = {tools},
    includes = {includes},
    compile_flags = {compile_flags},
    link_flags = {link_flags},
    version = {version},
)
""".format(
        config = repr(str(ctx.attr._config)),
        tools = repr(tools),
        includes = repr(includes),
        compile_flags = repr(manifest["compile_flags"]),
        link_flags = repr(manifest["link_flags"]),
        version = repr(version),
    ))

def _native_path(ctx, path):
    if not path.startswith("/") or not ctx.path(path).exists:
        return None
    real = str(ctx.path(path).realpath)
    if "ccache" in real or real.startswith("/nix/store/"):
        return None
    return real

def _native_impl(ctx):
    requested = ctx.os.environ.get("BASTION_GCC", "")
    candidates = [requested] if requested else [
        "/usr/bin/gcc-16", "/usr/bin/gcc16",
        "/usr/bin/gcc-15", "/usr/bin/gcc15",
        "/usr/bin/gcc-14", "/usr/bin/gcc14",
        "/usr/bin/gcc-13", "/usr/bin/gcc13",
        "/usr/bin/gcc",
    ]
    compiler = None
    version = None
    chosen = None
    for candidate in candidates:
        real = _native_path(ctx, candidate)
        if not real:
            continue
        result = ctx.execute([real, "-dumpfullversion"])
        detected = result.stdout.strip()
        if result.return_code == 0 and detected and int(detected.split(".")[0]) >= 13:
            compiler = real
            version = detected
            chosen = candidate
            break
    if not compiler:
        fail("Install native GCC/G++ 13 or newer (GCC 16 is preferred). " +
             "Expected a real compiler in /usr/bin, or set --repo_env=BASTION_GCC=/absolute/path/to/gcc. " +
             "ccache and Nix wrappers are not selected by the native toolchain.")

    tools = {"gcc": compiler}
    directory = str(ctx.path(chosen).dirname)
    filename = ctx.path(chosen).basename
    for name in _TOOLS:
        if name == "gcc":
            continue
        # Compiler companions must match versioned installs (e.g. gcov-16).
        companion = filename.replace("gcc", name)
        candidates = [directory + "/" + companion, directory + "/" + name]
        if name not in ["cpp", "gcov"]:
            candidates += ["/usr/bin/" + name]
        found = None
        for candidate in candidates:
            found = _native_path(ctx, candidate)
            if found:
                break
        if not found:
            fail("Missing native tool {}. Install GCC/G++ and binutils development packages.".format(name))
        tools[name] = found

    _materialize(ctx, {
        "schema_version": 1,
        "provider": "system",
        "compiler_version": version,
        "target": "x86_64-linux-gnu",
        "tools": tools,
        "include_directories": [],
        "compile_flags": [],
        "link_flags": [],
    })

def _nix_impl(ctx):
    manifest_path = ctx.os.environ.get("BASTION_GCC_TOOLCHAIN", "")
    if not manifest_path:
        fail("The optional --config=nix SDK needs BASTION_GCC_TOOLCHAIN. " +
             "Enter the Nix shell for this configuration, or omit --config=nix for a native build.")
    manifest = json.decode(ctx.read(manifest_path))
    if manifest.get("schema_version") != 1 or manifest.get("target") != "x86_64-linux-gnu":
        fail("Expected schema_version=1 and target=x86_64-linux-gnu in the toolchain manifest.")
    tools = manifest["tools"]
    for name in _TOOLS:
        path = tools.get(name, "")
        if not path.startswith("/nix/store/") or not ctx.path(path).exists:
            fail("{} must point to an existing tool in /nix/store, got: {}".format(name, path))
    version = _run(ctx, [tools["gcc"], "-dumpfullversion"]).stdout.strip()
    if not version.startswith("16.") or version != manifest["compiler_version"]:
        fail("Expected manifest's GCC 16 version {}, found {}".format(manifest["compiler_version"], version))
    _materialize(ctx, manifest)

_native_repository = repository_rule(
    implementation = _native_impl,
    environ = ["BASTION_GCC"],
    attrs = {"_config": attr.label(default = "//tools/toolchains:cc_toolchain_config.bzl")},
    local = True,
    configure = True,
)

_nix_repository = repository_rule(
    implementation = _nix_impl,
    environ = ["BASTION_GCC_TOOLCHAIN"],
    attrs = {"_config": attr.label(default = "//tools/toolchains:cc_toolchain_config.bzl")},
    local = True,
    configure = True,
)

def _extension_impl(_ctx):
    _native_repository(name = "bastion_gcc")
    _nix_repository(name = "bastion_gcc16")

gcc = module_extension(implementation = _extension_impl)
