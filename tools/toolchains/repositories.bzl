"""Materialize an explicit GCC 16 toolchain from the pinned Nix SDK manifest."""

_TOOLS = ["gcc", "cpp", "gcov", "ar", "ld", "nm", "objcopy", "objdump", "strip", "dwp"]

def _run(ctx, args):
    result = ctx.execute(
        args,
        environment = {
            # Probe the wrapper's intrinsic SDK, not incidental shell flags.
            "NIX_CFLAGS_COMPILE": "",
            "NIX_CFLAGS_LINK": "",
            "NIX_LDFLAGS": "",
        },
    )
    if result.return_code:
        fail("Toolchain probe failed: {}\n{}".format(args, result.stderr))
    return result

def _impl(ctx):
    manifest_path = ctx.os.environ.get("BASTION_GCC_TOOLCHAIN", "")
    if not manifest_path:
        fail("GCC 16 SDK is not configured. Run `nix develop path:.` first, then run Bazel. " +
             "The shell exports BASTION_GCC_TOOLCHAIN; no host/PATH compiler fallback is used.")
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
    triple = _run(ctx, [tools["gcc"], "-dumpmachine"]).stdout.strip()
    if not triple.startswith("x86_64-") or "linux" not in triple:
        fail("Compiler is not targeting x86_64 Linux: " + triple)

    # GCC reports libc, libstdc++, and compiler-internal header roots. These are
    # allowed by Bazel's include scanner without allowing all of /nix/store.
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

    ctx.file("toolchain.json", json.encode_indent(manifest))
    ctx.file("BUILD.bazel", """\
load({config}, "gcc16_toolchain")
package(default_visibility = ["//visibility:public"])
gcc16_toolchain(
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

_gcc_repository = repository_rule(
    implementation = _impl,
    environ = ["BASTION_GCC_TOOLCHAIN"],
    attrs = {"_config": attr.label(default = "//tools/toolchains:cc_toolchain_config.bzl")},
    local = True,
    configure = True,
)

def _extension_impl(_ctx):
    _gcc_repository(name = "bastion_gcc16")

gcc = module_extension(implementation = _extension_impl)
