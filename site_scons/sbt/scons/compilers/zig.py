from sbt import builder

def load_in_env(env):
    cmd_cc = "zig cc"
    cmd_cxx = "zig c++"
    
    target_triple = ""
    extra_flags = ""

    builder.libc = "musl"  # Ensure musl is used for zig builds

    if builder.is_cross_building:
        if builder.build_machine == "x86_64":
            target_triple = "x86_64-linux-musl"
        elif builder.build_machine == "i386" or builder.build_machine == "x86":
            target_triple = "i386-linux-musl"
        elif builder.build_machine == "arm":
            target_triple = "arm-linux-musleabihf"
            extra_flags = " -mcpu=generic+v7a" 
        elif builder.build_machine == "arm64":
            target_triple = "aarch64-linux-musl"
        elif builder.build_machine == "riscv64":
            target_triple = "riscv64-linux-musl"
        elif builder.build_machine == "riscv32":
            target_triple = "riscv32-linux-musl"

    if target_triple:
        target_arg = f" -target {target_triple}{extra_flags}"
        cmd_cc += target_arg
        cmd_cxx += target_arg

    env.Replace(
        CC = cmd_cc,
        CXX = cmd_cxx,
        AR = "zig ar",
        RANLIB = "zig ranlib",
    )

    # Use LLVM's lld linker for better cross-compilation support
    env.Append(
        LINKFLAGS = ['-fuse-ld=lld']
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
