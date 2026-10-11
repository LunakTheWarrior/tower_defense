"""GCC 16 policy and native Linux x86_64 C/C++ toolchain registration.

Reuse rules_cc's tested Unix action/feature definitions for dependency files,
response files, PIC, archives, shared libraries, coverage, and linking. Keep
our architecture, language, diagnostics, hardening, and mode policy here.
Compiler and SDK paths are validated by repositories.bzl, not resolved in PATH.
"""

load("@rules_cc//cc:defs.bzl", "cc_toolchain")
load("@rules_cc//cc/private/toolchain:unix_cc_toolchain_config.bzl", "cc_toolchain_config")

def gcc16_toolchain(name, tools, includes, compile_flags, link_flags, version):
    """Declare the configured compiler and constrain both execution and target."""
    identifier = "bastion-gcc-{}-x86_64-linux-gnu".format(version)
    cc_toolchain_config(
        name = name + "_config",
        toolchain_identifier = identifier,
        host_system_name = "x86_64-linux-gnu",
        target_system_name = "x86_64-linux-gnu",
        cpu = "k8",
        target_libc = "glibc",
        compiler = "gcc",
        abi_version = "gcc-16",
        abi_libc_version = "glibc",
        tool_paths = tools,
        cxx_builtin_include_directories = includes,
        compile_flags = [
            "-march=x86-64",
            "-mtune=generic",
            "-Wall",
            "-Wextra",
            "-fstack-protector-strong",
            "-fno-omit-frame-pointer",
            "-ffunction-sections",
            "-fdata-sections",
        ] + compile_flags,
        cxx_flags = ["-std=c++23"],
        conly_flags = ["-std=gnu99"],
        dbg_compile_flags = ["-O0", "-g3", "-D_GLIBCXX_ASSERTIONS"],
        opt_compile_flags = ["-O2", "-DNDEBUG", "-U_FORTIFY_SOURCE", "-D_FORTIFY_SOURCE=2"],
        opt_link_flags = ["-Wl,--gc-sections"],
        link_flags = [
            "-march=x86-64",
            "-Wl,-z,relro",
            "-Wl,-z,now",
            "-Wl,--build-id=sha1",
        ] + link_flags,
        link_libs = ["-lstdc++", "-lm"],
        coverage_compile_flags = ["--coverage"],
        coverage_link_flags = ["--coverage"],
        unfiltered_compile_flags = [
            "-no-canonical-prefixes",
            "-Wno-builtin-macro-redefined",
            '-D__DATE__="redacted"',
            '-D__TIME__="redacted"',
            '-D__TIMESTAMP__="redacted"',
        ],
        supports_start_end_lib = False,
    )
    native.filegroup(
        name = name + "_inputs",
        srcs = ["toolchain.json"],
    )
    native.filegroup(name = name + "_empty")
    cc_toolchain(
        name = name + "_cc",
        toolchain_identifier = identifier,
        toolchain_config = ":" + name + "_config",
        all_files = ":" + name + "_inputs",
        ar_files = ":" + name + "_inputs",
        as_files = ":" + name + "_inputs",
        compiler_files = ":" + name + "_inputs",
        dwp_files = ":" + name + "_inputs",
        linker_files = ":" + name + "_inputs",
        objcopy_files = ":" + name + "_inputs",
        strip_files = ":" + name + "_inputs",
        supports_param_files = 1,
    )
    native.toolchain(
        name = name,
        toolchain = ":" + name + "_cc",
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
        exec_compatible_with = ["@platforms//cpu:x86_64", "@platforms//os:linux"],
        target_compatible_with = ["@platforms//cpu:x86_64", "@platforms//os:linux"],
    )
