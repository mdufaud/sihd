from sbt import builder

def load_in_env(env):
    prefix = ""
    if builder.is_cross_building:
        if builder.build_machine == "x86_64":
            prefix = "x86_64-linux-gnu-"
        elif builder.build_machine == "i386" or builder.build_machine == "x86":
            prefix = "i686-linux-gnu-"
        elif builder.build_machine == "arm":
            # hardfloat
            prefix = "arm-linux-gnueabihf-"
        elif builder.build_machine == "arm64":
            prefix = "aarch64-linux-gnu-"
        elif builder.build_machine == "riscv64":
            prefix = "riscv64-linux-gnu-"
        elif builder.build_machine == "riscv32":
            prefix = "riscv32-linux-gnu-"

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