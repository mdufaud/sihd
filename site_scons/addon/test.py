def generate_test_env(builder):
    return {
        # Python bindings load the interpreter from the vcpkg extlib dir.
        "PYTHONHOME": builder.build_extlib_path,
        # libcurl resolves its proxy from the environment on every handle: tests
        # talk to local servers and must not be routed through a developer proxy
        "NO_PROXY": "*",
        "no_proxy": "*",
        # the line test::TestWatchdog prints before killing a hung binary: names the test in brief runs
        "TEST_INTERRUPT_PATTERN": r"^(\S+) timed out after \d+s$",
    }
