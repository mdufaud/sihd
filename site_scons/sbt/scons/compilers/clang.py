from sbt import builder

def load_in_env(env):
    if builder.is_cross_building():
        gnu_triplet = builder.get_gnu_triplet()
        env.Append(
            CPPFLAGS = [f"--target={gnu_triplet}"],
            LINKFLAGS = [f"--target={gnu_triplet}"],
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

    # CPU target (-march=/-mcpu= depending on architecture)
    cpu_flags = builder.get_cpu_flags(builder.build_machine, builder.build_cpu)
    if cpu_flags:
        env.Append(CPPFLAGS = cpu_flags)
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
    if builder.build_ubsan:
        ubsan_flags = ["-fsanitize=undefined", "-fno-omit-frame-pointer"]
        env.Append(CPPFLAGS = ubsan_flags, LINKFLAGS = ubsan_flags)
    if builder.build_tsan:
        tsan_flags = ["-fsanitize=thread", "-fno-omit-frame-pointer"]
        env.Append(CPPFLAGS = tsan_flags, LINKFLAGS = tsan_flags)
    if builder.build_lsan:
        lsan_flags = ["-fsanitize=leak"]
        env.Append(CPPFLAGS = lsan_flags, LINKFLAGS = lsan_flags)
    if builder.build_msan:
        msan_flags = ["-fsanitize=memory", "-fno-omit-frame-pointer", "-fsanitize-memory-track-origins"]
        env.Append(CPPFLAGS = msan_flags, LINKFLAGS = msan_flags)
    if builder.build_hwasan and builder.build_machine == "arm64":
        hwasan_flags = ["-fsanitize=hwaddress", "-fno-omit-frame-pointer"]
        env.Append(CPPFLAGS = hwasan_flags, LINKFLAGS = hwasan_flags)
    if builder.build_coverage:
        coverage_flags = ["--coverage", "-fprofile-arcs", "-ftest-coverage"]
        env.Append(CPPFLAGS = coverage_flags, LINKFLAGS = coverage_flags)

    if builder.build_static_libs and builder.build_asan:
        env.Append(
            LINKFLAGS = ["-static-libasan"]
        )
