def generate_test_env(builder):
    return {
        # Python bindings load the interpreter from the vcpkg extlib dir.
        "PYTHONHOME": builder.build_extlib_path,
    }
