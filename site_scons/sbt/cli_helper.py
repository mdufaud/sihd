import os
import json

import builder
import loader


def _runner_env():
    env = {}
    for entry in builder.get_test_runner_env():
        key, _, val = entry.partition("=")
        env[key] = val
    return env


def _test_env(app):
    fn = getattr(app, "generate_test_env", None)
    if fn is not None:
        if callable(fn):
            return fn(builder)
        else:
            raise TypeError(f"app.generate_test_env is not callable: {fn}")
    return {}


def _relative_paths(build_path):
    abs_paths = {
        "bin": builder.build_bin_path,
        "lib": builder.build_lib_path,
        "include": builder.build_hdr_path,
        "etc": builder.build_etc_path,
        "share": builder.build_share_path,
        "test": builder.build_test_path,
        "test_bin": os.path.join(builder.build_test_path, "bin"),
        "demo": builder.build_demo_path,
        "obj": builder.build_obj_path,
        "extlib": builder.build_extlib_path,
        "extlib_lib": builder.build_extlib_lib_path,
    }
    return {k: os.path.relpath(v, build_path) for k, v in abs_paths.items()}


def _build_env():
    app = loader.load_app()
    return {
        "app_name": app.name,
        "root_path": builder.build_root_path,
        "build_path": builder.build_path,
        "build": {
            "platform": builder.build_platform,
            "machine": builder.build_machine,
            "mode": builder.build_mode,
            "compiler": builder.build_compiler_version,
            "triplet": builder.get_gnu_triplet(),
            "libs_type": builder.libs_type,
        },
        "paths": _relative_paths(builder.build_path),
        "test": {
            "runner": builder.get_test_runner() or "",
            "bin_ext": builder.get_test_bin_ext() or "",
            "runner_env": _runner_env(),
            "env": _test_env(app),
        },
    }


def _dump():
    os.makedirs(builder.build_path, exist_ok=True)
    json_path = os.path.join(builder.build_path, "sbt_env.json")
    with open(json_path, "w") as f:
        json.dump(_build_env(), f, indent=4)
    return json_path


if __name__ == '__main__':
    import sys

    if len(sys.argv) != 2:
        sys.exit(0)
    if sys.argv[1] == "dump":
        print(_dump())
    elif sys.argv[1] == "compiler":
        print(builder.build_compiler_version)
    elif sys.argv[1] == "platform":
        print(builder.build_platform)
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
    elif sys.argv[1] == "runner":
        print(builder.get_test_runner())
    elif sys.argv[1] == "runner_env":
        print("\n".join(builder.get_test_runner_env()))
