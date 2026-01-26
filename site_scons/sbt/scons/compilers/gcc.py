from sbt import builder

def load_in_env(env):
    if builder.use_zig:
        cmd_cc = "zig cc"
        cmd_cxx = "zig c++"
        
        target_triple = ""
        extra_flags = ""

        if builder.is_cross_building:
            if builder.build_machine == "x86_64":
                target_triple = "x86_64-linux-musl"
            elif builder.build_machine == "arm":
                target_triple = "arm-linux-musleabihf"
                extra_flags = " -mcpu=generic+v7a" 
            elif builder.build_machine == "arm64":
                target_triple = "aarch64-linux-musl"
            elif builder.build_machine == "riscv64":
                target_triple = "riscv64-linux-musl"
            elif builder.build_machine == "riscv32":
                target_triple = "riscv32-linux-musl"

        # if builder.build_architecture == "32":
        #     env.Append(
        #         CPPFLAGS = ["-m32"],
        #         LINKFLAGS = ["-m32"]
        #     )

        if target_triple:
            target_arg = " -target {}{}".format(target_triple, extra_flags)
            cmd_cc += target_arg
            cmd_cxx += target_arg

        env.Replace(
            CC = cmd_cc,
            CXX = cmd_cxx,
            AR = "zig ar",
            RANLIB = "zig ranlib",
        )
        env.Append(
            LINKFLAGS = ['-fuse-ld=lld']
        )
    else:
        prefix = ""
        if builder.is_cross_building:
            if builder.build_machine == "x86_64":
                prefix = "x86_64-linux-gnu-"
            elif builder.build_machine == "arm":
                # hardfloat
                prefix = "arm-linux-gnueabihf-"
            elif builder.build_machine == "arm64":
                prefix = "aarch64-linux-gnu-"
            elif builder.build_machine == "riscv64":
                prefix = "riscv64-linux-gnu-"
            elif builder.build_machine == "riscv32":
                prefix = "riscv32-linux-gnu-"
        # if builder.build_architecture == "32":
        #     env.Append(
        #         CPPFLAGS = ["-m32"],
        #         LINKFLAGS = ["-m32"]
        #     )
        env.Replace(
            CC = prefix + "gcc",
            CXX = prefix + "g++",
            AR = prefix + "ar",
            RANLIB = prefix + "ranlib",
        )
    if builder.build_asan:
        gcc_asan_flags = [
            "-fsanitize=address", # With gcc - has leak enabled by default
            "-fno-omit-frame-pointer", # Leave frame pointers. Allows the fast unwinder to function properly.
            "-fno-common", # helps detect global variables issues
            "-fno-inline" # readable stack traces
        ]
        env.Append(
            CPPFLAGS = gcc_asan_flags,
            LINKFLAGS = gcc_asan_flags
        )
    if builder.build_static_libs:
        env.Append(
            LINKFLAGS = ["-static", "-static-libgcc", "-static-libstdc++"]
        )
    if builder.build_static_libs and builder.build_asan:
        env.Append(
            LINKFLAGS = ["-static-libasan"]
        )