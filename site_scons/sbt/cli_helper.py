if __name__ == '__main__':
    import sys
    import builder

    if len(sys.argv) != 2:
        sys.exit(0)
    if sys.argv[1] == "compiler":
        print(builder.build_compiler)
    elif sys.argv[1] == "platform":
        print(builder.build_platform)
    elif sys.argv[1] == "arch":
        print(builder.build_architecture)
    elif sys.argv[1] == "machine":
        print(builder.build_machine)
    elif sys.argv[1] == "mode":
        print(builder.build_mode)
    elif sys.argv[1] == "termux":
        print(builder.build_on_termux and "true" or "false")
    elif sys.argv[1] == "path":
        print(builder.build_path)
    elif sys.argv[1] == "static":
        print(builder.libs_type)
    elif sys.argv[1] == "triplet":
        print(builder.get_gnu_triplet())
    elif sys.argv[1] == "all":
        print(" ".join([
            builder.build_platform,
            builder.build_machine,
            builder.build_mode,
            builder.build_architecture,
            builder.build_compiler,
            builder.get_gnu_triplet(),
            builder.build_on_termux and "true" or "false",
            builder.build_path,
        ]))
