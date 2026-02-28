from sbt import builder
from sbt import architectures

def load_in_env(env):
    cmd_cc = "zig cc"
    cmd_cxx = "zig c++"
    
    target_triple = ""
    extra_flags = ""

    builder.libc = "musl"  # Zig always uses musl

    if builder.is_cross_building():
        target_triple = architectures.get_zig_target(builder.build_machine)
        extra_flags = architectures.get_zig_flags(builder.build_machine)
        if not target_triple:
            raise SystemExit(f"No Zig target for machine={builder.build_machine}")

    if target_triple:
        target_arg = f" -target {target_triple}"
        if extra_flags:
            target_arg += f" {extra_flags}"
        cmd_cc += target_arg
        cmd_cxx += target_arg

    # CPU target (-march=/-mcpu= depending on architecture)
    cpu_flags = builder.get_cpu_flags(builder.build_machine, builder.build_cpu)
    if cpu_flags:
        cpu_str = " ".join(cpu_flags)
        cmd_cc += f" {cpu_str}"
        cmd_cxx += f" {cpu_str}"

    env.Replace(
        CC = cmd_cc,
        CXX = cmd_cxx,
        AR = "zig ar",
        RANLIB = "zig ranlib",
    )

    # Use LLVM's lld linker for better cross-compilation support
    # Disable ubsan to avoid missing runtime symbols
    env.Append(
        CPPFLAGS = ['-fno-sanitize=undefined'],
        LINKFLAGS = ['-fuse-ld=lld', '-fno-sanitize=undefined']
    )

    if builder.build_asan:
        zig_asan_flags = [
            "-fsanitize=address",
            "-fno-omit-frame-pointer",
            "-fno-common",
            "-fno-inline"
        ]
        env.Append(
            CPPFLAGS = zig_asan_flags,
            LINKFLAGS = zig_asan_flags
        )

    if builder.build_static_libs:
        env.Append(
            LINKFLAGS = ["-static"]
        )

    if builder.build_static_libs and builder.build_asan:
        env.Append(
            LINKFLAGS = ["-static-libasan"]
        )
