import sys
import json
import os

from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

sys.dont_write_bytecode = True
import app
from site_scons.scripts import modules as modules
from site_scons.scripts import builder as builder

vcpkg_dir_path = os.path.join(builder.build_root_path, ".vcpkg")
vcpkg_bin_path = os.path.join(vcpkg_dir_path, "vcpkg")

if builder.is_msys():
    vcpkg_bin_path += ".exe"

vcpkg_build_path = os.path.join(builder.build_path, "vcpkg")
vcpkg_build_manifest_path = os.path.join(vcpkg_build_path, "vcpkg.json")

if builder.verify_args(app) == False:
    exit(1)

modules_to_build = builder.get_modules()
modules_forced_to_build = builder.get_force_build_modules()

modules_lst = builder.get_modules_lst()
if modules_forced_to_build:
    modules_lst.extend(builder.get_force_build_modules_lst())

build_platform = builder.build_platform
verbose = builder.build_verbose
has_test = builder.build_tests

if verbose:
    if modules_lst:
        builder.debug("getting libs from modules -> {}".format(modules_lst))
    if has_test:
        builder.debug("including test libs")

skip_libs = getattr(app, "extlibs_skip", [])
extlibs = {}

if modules_to_build != "NONE":
    # Get modules configuration for this build
    try:
        build_modules = modules.build_modules_conf(app, specific_modules=modules_lst)
    except RuntimeError as e:
        builder.error(str(e))
        exit(1)

    # Checking module availability on platforms
    deleted_modules = modules.check_platform(build_modules, build_platform)
    for deleted_modules in deleted_modules:
        builder.warning("module '{}' cannot compile on platform: {}".format(deleted_modules, build_platform))

    if not build_modules:
        exit(0)

    if verbose:
        builder.debug("modules configuration: ")
        pp.pprint(build_modules)
        print()

    extlibs.update(modules.get_modules_extlibs(app, build_modules, build_platform))
    if has_test and hasattr(app, "test_extlibs"):
        extlibs.update(modules.get_extlibs_versions(app, app.test_extlibs))

    for skip_lib in skip_libs:
        if skip_lib in extlibs:
            del extlibs[skip_lib]

    if verbose:
        builder.debug("modules external libs:")
        pp.pprint(extlibs)
        print()

def build_vcpkg_triplet():
    vcpkg_triplet = builder.get_opt("triplet", "")

    if len(vcpkg_triplet) == 0:
        if builder.build_architecture == "x86_64":
            vcpkg_triplet = "x64"
        else:
            vcpkg_triplet = builder.build_architecture

        if builder.build_platform == "windows":
            vcpkg_triplet += f"-mingw"
        else:
            vcpkg_triplet += f"-{builder.build_platform}"

        # no community triplet for static/dynamic here
        if vcpkg_triplet != "arm64-linux":
            if builder.build_static_libs:
                vcpkg_triplet += "-static"
            else:
                vcpkg_triplet += "-dynamic"

    return vcpkg_triplet

vcpkg_triplet = build_vcpkg_triplet()

def build_vcpkg_manifest():
    vcpkg_baseline = "7977f0a771e64e9811d32aa30d9a247e09c39b2e"
    if hasattr(app, "vcpkg_baseline"):
        vcpkg_baseline = app.vcpkg_baseline
    vcpkg_manifest = {
        "builtin-baseline": f"{vcpkg_baseline}",
        "dependencies": [
        ],
        "overrides": [
        ],
    }

    features = getattr(app, "extlibs_features", {})
    for name, feature_list in getattr(app, f"extlibs_features_{builder.build_platform}", {}).items():
        if name in features:
            features[name].extend(feature_list)
        else:
            features[name] = feature_list

    def add_dependency(name, version):
        if name in features:
            vcpkg_manifest["dependencies"].append({
                "name": name,
                "features": features.get(name)
            })
        else:
            vcpkg_manifest["dependencies"].append(name)
        vcpkg_manifest["overrides"].append({
            "name": name,
            "version": version,
        })
    for libname, libversion in extlibs.items():
        add_dependency(libname, libversion)
    return vcpkg_manifest

def write_vcpkg_manifest(vcpkg_manifest):
    os.makedirs(vcpkg_build_path, exist_ok=True)
    with open(vcpkg_build_manifest_path, "w") as fd:
        json.dump(vcpkg_manifest, fd, indent=2)
    builder.info(f"Wrote vcpkg manifest at: {vcpkg_build_manifest_path}")

def __check_vcpkg():
    if os.path.exists(vcpkg_bin_path) is False:
        builder.error(f"VCPKG path does not exist: {vcpkg_bin_path}")
        builder.error(f"Deploy VCPKG with: make vcpkg deploy")
        raise RuntimeError("No VCPKG installed")

def execute_vcpkg_install():
    __check_vcpkg()
    copy_env = os.environ.copy()
    args = [vcpkg_bin_path, "install", f"--triplet={vcpkg_triplet}", "--allow-unsupported"]

    if builder.is_msys():
        args += ["--host-triplet=x64-mingw-dynamic"]

    if "arm" in vcpkg_triplet:
        copy_env["VCPKG_FORCE_SYSTEM_BINARIES"] = "1"
        def check_bins(list):
            import shutil
            ret = True
            for name in list:
                if not shutil.which(name):
                    builder.error(f"Binary {name} is not present but needed")
                    ret = False
            return ret
        assert(check_bins(["cmake", "ninja", "pkg-config"]))

    if verbose:
        builder.debug(f"executing '{args}' in '{vcpkg_build_path}'")
    import subprocess
    proc = subprocess.run(args, cwd=vcpkg_build_path, timeout=(120.0 * len(extlibs)), env=copy_env)
    return proc.returncode

def execute_vcpkg_depend_info():
    __check_vcpkg()
    args = [vcpkg_bin_path, "depend-info"] + list(extlibs.keys())
    args += [f"--triplet={vcpkg_triplet}", "--format=tree", "--max-recurse=-1"]
    if verbose:
        builder.debug(f"executing '{args}' in '{vcpkg_build_path}'")
    import subprocess
    proc = subprocess.run(args, cwd=vcpkg_build_path)
    return proc.returncode

def execute_vcpkg_list():
    __check_vcpkg()
    args = (vcpkg_bin_path, "list", f"--triplet={vcpkg_triplet}")
    if verbose:
        builder.debug(f"executing '{args}' in '{vcpkg_build_path}'")
    import subprocess
    proc = subprocess.run(args, cwd=vcpkg_build_path)
    return proc.returncode

def link_to_extlibs():
    downloaded_path = os.path.join(vcpkg_build_path, "vcpkg_installed", vcpkg_triplet)
    if os.path.exists(downloaded_path):
        # remove existing link:
        if os.path.islink(builder.build_extlib_path):
            os.unlink(builder.build_extlib_path)
        if os.path.exists(builder.build_extlib_path):
            os.unlink(builder.build_extlib_path)
        os.symlink(downloaded_path, builder.build_extlib_path, target_is_directory=True)

if __name__ == "__main__":
    vcpkg_manifest = build_vcpkg_manifest()
    write_vcpkg_manifest(vcpkg_manifest)
    if "fetch" in sys.argv:
        builder.info("fetching external libraries for {}".format(app.name))
        return_code = execute_vcpkg_install()
        if return_code == 0:
            link_to_extlibs()
        sys.exit(return_code)
    elif "list" in sys.argv:
        execute_vcpkg_list()
    elif "info" in sys.argv:
        execute_vcpkg_depend_info()
