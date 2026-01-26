from sbt import builder

def load_in_env(env):
    if builder.is_cross_building:
        gnu_triplet = builder.get_gnu_triplet()
        gnu_triplet = gnu_triplet.replace("arm64", "aarch64")
        env.Append(
            CPPFLAGS = [f"--target={gnu_triplet}"],
            LINKFLAGS = [f"--target={gnu_triplet}"],
        )
    elif builder.build_architecture == "32":
        env.Append(
            CPPFLAGS = ["-m32"],
            LINKFLAGS = ["-m32"]
        )

    env.Replace(
        # compiler for c
        CC = "clang",
        # compiler for c++
        CXX = "clang++",
        # static library archiver
        AR = "ar",
        # static library indexer
        RANLIB = "ranlib",
    )
    if builder.build_asan:
        clang_asan_flags = [
            "-fsanitize=address,leak",
            "-fno-omit-frame-pointer", # Leave frame pointers. Allows the fast unwinder to function properly.
            "-fno-common", # helps detect global variables issues
            "-fno-inline" # readable stack traces
        ]
        env.Append(
            CPPFLAGS = clang_asan_flags,
            LINKFLAGS = clang_asan_flags
        )
    if builder.build_static_libs:
        env.ParseConfig("llvm-config --libs --system-libs --link-static")
    else:
        env.ParseConfig("llvm-config --libs --ldflags --system-libs")

    if builder.build_static_libs and builder.build_asan:
        env.Append(
            LINKFLAGS = ["-static-libasan"]
        )
